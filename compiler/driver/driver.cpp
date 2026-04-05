#include "compiler/driver/driver.h"

#include <array>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include "compiler/ast/ast.h"
#include "compiler/codegen_llvm/backend.h"
#include "compiler/driver/project.h"
#include "compiler/lex/lexer.h"
#include "compiler/mci/mci.h"
#include "compiler/mir/mir.h"
#include "compiler/parse/parser.h"
#include "compiler/sema/check.h"
#include "compiler/support/assert.h"
#include "compiler/support/diagnostics.h"
#include "compiler/support/dump_paths.h"
#include "compiler/support/source_manager.h"

namespace mc::driver {
namespace {

constexpr std::string_view kUsage =
    "Modern C bootstrap driver\n"
    "\n"
    "Usage:\n"
    "  mc check <source> [--build-dir <dir>] [--import-root <dir>] [--emit-dump-paths] [--dump-ast] [--dump-mir] [--dump-backend]\n"
    "  mc check [--project <build.toml>] [--target <name>] [--mode <name>] [--env <name>] [--build-dir <dir>] [--import-root <dir>] [--emit-dump-paths] [--dump-ast] [--dump-mir] [--dump-backend]\n"
    "  mc build <source> [--build-dir <dir>] [--import-root <dir>] [--emit-dump-paths] [--dump-ast] [--dump-mir] [--dump-backend]\n"
    "  mc build [--project <build.toml>] [--target <name>] [--mode <name>] [--env <name>] [--build-dir <dir>] [--import-root <dir>] [--emit-dump-paths] [--dump-ast] [--dump-mir] [--dump-backend]\n"
    "  mc run <source> [--build-dir <dir>] [--import-root <dir>] [-- args...]\n"
    "  mc run [--project <build.toml>] [--target <name>] [--mode <name>] [--env <name>] [--build-dir <dir>] [--import-root <dir>] [-- args...]\n"
    "  mc test [--project <build.toml>] [--target <name>] [--mode <name>] [--env <name>] [--build-dir <dir>] [--import-root <dir>]\n"
    "  mc dump-paths <source> [--build-dir <dir>]\n"
    "  mc --help\n";

struct CommandOptions {
    std::filesystem::path source_path;
    std::optional<std::filesystem::path> project_path;
    std::optional<std::string> target_name;
    std::optional<std::string> mode_override;
    std::optional<std::string> env_override;
    std::filesystem::path build_dir = "build/debug";
    bool build_dir_explicit = false;
    std::vector<std::filesystem::path> import_roots;
    std::vector<std::string> run_arguments;
    bool emit_dump_paths = false;
    bool dump_ast = false;
    bool dump_mir = false;
    bool dump_backend = false;
};

enum class InvocationKind {
    kDirectSource,
    kProjectTarget,
};

InvocationKind ClassifyInvocation(const CommandOptions& options) {
    return options.project_path.has_value() || options.target_name.has_value() || options.source_path.empty()
               ? InvocationKind::kProjectTarget
               : InvocationKind::kDirectSource;
}

constexpr std::string_view kHostedRuntimeSupportRelativePath = "runtime/hosted/mc_hosted_runtime.c";

std::optional<std::filesystem::path> DiscoverRepositoryRoot(const std::filesystem::path& start_path) {
    std::filesystem::path current = std::filesystem::absolute(start_path).lexically_normal();
    if (!std::filesystem::is_directory(current)) {
        current = current.parent_path();
    }

    while (!current.empty()) {
        if (std::filesystem::exists(current / "stdlib") && std::filesystem::exists(current / "runtime")) {
            return current;
        }
        if (current == current.root_path()) {
            break;
        }
        current = current.parent_path();
    }

    return std::nullopt;
}

std::optional<std::filesystem::path> DiscoverHostedRuntimeSupportSource(const std::filesystem::path& source_path) {
    const auto repo_root = DiscoverRepositoryRoot(source_path);
    if (!repo_root.has_value()) {
        return std::nullopt;
    }

    const std::filesystem::path runtime_source = *repo_root / std::string(kHostedRuntimeSupportRelativePath);
    if (!std::filesystem::exists(runtime_source)) {
        return std::nullopt;
    }
    return runtime_source;
}

std::vector<std::filesystem::path> ComputeEffectiveImportRoots(const std::filesystem::path& source_path,
                                                               const std::vector<std::filesystem::path>& import_roots) {
    std::vector<std::filesystem::path> resolved_roots;
    resolved_roots.reserve(import_roots.size() + 1);

    std::unordered_set<std::string> seen_roots;
    const auto add_root = [&](const std::filesystem::path& root) {
        const std::filesystem::path normalized = std::filesystem::absolute(root).lexically_normal();
        const std::string key = normalized.generic_string();
        if (seen_roots.insert(key).second) {
            resolved_roots.push_back(normalized);
        }
    };

    for (const auto& import_root : import_roots) {
        add_root(import_root);
    }

    if (const auto repo_root = DiscoverRepositoryRoot(source_path); repo_root.has_value()) {
        const std::filesystem::path stdlib_root = *repo_root / "stdlib";
        if (std::filesystem::exists(stdlib_root)) {
            add_root(stdlib_root);
        }
    }

    return resolved_roots;
}

std::optional<std::string> ReadSourceText(const std::filesystem::path& path,
                                          support::DiagnosticSink& diagnostics) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        diagnostics.Report({
            .file_path = path,
            .span = support::kDefaultSourceSpan,
            .severity = support::DiagnosticSeverity::kError,
            .message = "unable to read source file",
        });
        return std::nullopt;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    if (!input.good() && !input.eof()) {
        diagnostics.Report({
            .file_path = path,
            .span = support::kDefaultSourceSpan,
            .severity = support::DiagnosticSeverity::kError,
            .message = "unable to read source file",
        });
        return std::nullopt;
    }

    return buffer.str();
}

std::optional<mc::parse::ParseResult> ParseSourcePath(const std::filesystem::path& path,
                                                      support::DiagnosticSink& diagnostics) {
    const auto source_text = ReadSourceText(path, diagnostics);
    if (!source_text.has_value()) {
        return std::nullopt;
    }

    const auto lex_result = mc::lex::Lex(*source_text, path.generic_string(), diagnostics);
    auto parse_result = mc::parse::Parse(lex_result, path, diagnostics);
    if (!parse_result.ok) {
        return std::nullopt;
    }

    return parse_result;
}

mc::sema::CheckOptions BuildSemaOptions(const std::filesystem::path& source_path,
                                        const CommandOptions& options) {
    mc::sema::CheckOptions sema_options;
    sema_options.import_roots = ComputeEffectiveImportRoots(source_path, options.import_roots);
    return sema_options;
}

void PrintUsage(std::ostream& stream) {
    stream << kUsage;
}

std::string JoinNames(const std::vector<std::string>& names) {
    std::string text;
    for (std::size_t index = 0; index < names.size(); ++index) {
        if (index > 0) {
            text += ", ";
        }
        text += names[index];
    }
    return text;
}

std::vector<std::string> CollectEnabledTestTargetNames(const ProjectFile& project) {
    std::vector<std::string> names;
    for (const auto& [name, target] : project.targets) {
        if (target.tests.enabled) {
            names.push_back(name);
        }
    }
    std::sort(names.begin(), names.end());
    return names;
}

bool UsesProjectOnlyOptions(const CommandOptions& options) {
    return options.project_path.has_value() || options.target_name.has_value() || options.mode_override.has_value() ||
           options.env_override.has_value();
}

bool ValidateInvocationForCommand(std::string_view command,
                                  const CommandOptions& options,
                                  std::ostream& errors) {
    if (command == "test") {
        if (!options.source_path.empty()) {
            errors << "mc test does not accept a direct source path; use --project <build.toml>\n";
            return false;
        }
        return true;
    }

    if (command == "dump-paths") {
        if (UsesProjectOnlyOptions(options)) {
            errors << "mc dump-paths only supports direct-source mode; remove project-only options\n";
            return false;
        }
        if (options.source_path.empty()) {
            errors << "mc dump-paths requires a source path\n";
            return false;
        }
        return true;
    }

    if (!options.source_path.empty() && UsesProjectOnlyOptions(options)) {
        errors << "cannot mix a direct source path with project-only options; choose direct-source mode or --project mode\n";
        return false;
    }

    return true;
}

std::optional<CommandOptions> ParseCommandOptions(int argc,
                                                  const char* argv[],
                                                  int start_index,
                                                  std::ostream& errors) {
    CommandOptions options;
    int index = start_index;

    struct BoolFlag {
        std::string_view name;
        bool CommandOptions::*member;
    };
    constexpr std::array<BoolFlag, 4> bool_flags {{
        {"--emit-dump-paths", &CommandOptions::emit_dump_paths},
        {"--dump-ast", &CommandOptions::dump_ast},
        {"--dump-mir", &CommandOptions::dump_mir},
        {"--dump-backend", &CommandOptions::dump_backend},
    }};

    const auto require_value = [&](std::string_view flag) -> std::optional<std::string_view> {
        if (index + 1 >= argc) {
            errors << "missing value for " << flag << '\n';
            return std::nullopt;
        }
        return std::string_view(argv[++index]);
    };

    if (index < argc) {
        const std::string_view first = argv[index];
        if (!first.empty() && !first.starts_with("--")) {
            options.source_path = argv[index++];
        }
    }

    for (; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--") {
            for (++index; index < argc; ++index) {
                options.run_arguments.push_back(argv[index]);
            }
            break;
        }

        bool matched_flag = false;
        for (const auto& flag : bool_flags) {
            if (argument == flag.name) {
                (options.*(flag.member)) = true;
                matched_flag = true;
                break;
            }
        }
        if (matched_flag) {
            continue;
        }

        if (argument == "--build-dir") {
            const auto value = require_value(argument);
            if (!value.has_value()) {
                return std::nullopt;
            }
            options.build_dir = *value;
            options.build_dir_explicit = true;
            continue;
        }

        if (argument == "--project") {
            const auto value = require_value(argument);
            if (!value.has_value()) {
                return std::nullopt;
            }
            options.project_path = std::filesystem::path(*value);
            continue;
        }

        if (argument == "--target") {
            const auto value = require_value(argument);
            if (!value.has_value()) {
                return std::nullopt;
            }
            options.target_name = std::string(*value);
            continue;
        }

        if (argument == "--mode") {
            const auto value = require_value(argument);
            if (!value.has_value()) {
                return std::nullopt;
            }
            options.mode_override = std::string(*value);
            continue;
        }

        if (argument == "--env") {
            const auto value = require_value(argument);
            if (!value.has_value()) {
                return std::nullopt;
            }
            options.env_override = std::string(*value);
            continue;
        }

        if (argument == "--import-root") {
            const auto value = require_value(argument);
            if (!value.has_value()) {
                return std::nullopt;
            }
            options.import_roots.push_back(*value);
            continue;
        }

        errors << "unknown argument: " << argument << '\n';
        return std::nullopt;
    }

    if (ClassifyInvocation(options) == InvocationKind::kDirectSource && options.source_path.empty()) {
        errors << "missing source path\n";
        return std::nullopt;
    }

    return options;
}

void PrintDumpTargets(const support::DumpTargets& targets, std::ostream& stream) {
    stream << "ast: " << targets.ast.generic_string() << '\n';
    stream << "mir: " << targets.mir.generic_string() << '\n';
    stream << "backend: " << targets.backend.generic_string() << '\n';
    stream << "mci: " << targets.mci.generic_string() << '\n';
}

void PrintArtifactTargets(const support::DumpTargets& dump_targets,
                          const support::BuildArtifactTargets& build_targets,
                          std::ostream& stream) {
    PrintDumpTargets(dump_targets, stream);
    stream << "llvm: " << build_targets.llvm_ir.generic_string() << '\n';
    stream << "object: " << build_targets.object.generic_string() << '\n';
    stream << "executable: " << build_targets.executable.generic_string() << '\n';
    stream << "staticlib: " << build_targets.static_library.generic_string() << '\n';
}

struct BuildUnit {
    std::filesystem::path source_path;
    mc::parse::ParseResult parse_result;
    mc::sema::CheckResult sema_result;
    mc::mir::LowerResult mir_result;
    std::filesystem::path llvm_ir_path;
    std::filesystem::path object_path;
    std::string interface_hash;
    bool reused_object = false;
};

struct ImportedInterfaceData {
    std::unordered_map<std::string, sema::Module> modules;
    std::vector<std::pair<std::string, std::string>> interface_hashes;
};

struct ModuleBuildState {
    std::string target_identity;
    std::string mode;
    std::string env;
    std::string source_hash;
    std::string interface_hash;
    bool wrap_hosted_main = false;
    std::vector<std::pair<std::string, std::string>> imported_interface_hashes;
};

constexpr int kModuleBuildStateFormatVersion = 1;

struct CompileNode {
    std::string module_name;
    std::filesystem::path source_path;
    mc::parse::ParseResult parse_result;
    std::vector<std::pair<std::string, std::filesystem::path>> imports;
};

struct CompileGraph {
    std::filesystem::path entry_source_path;
    std::vector<std::filesystem::path> import_roots;
    std::filesystem::path build_dir;
    std::string mode;
    std::string env;
    bool wrap_entry_main = true;
    mc::codegen_llvm::TargetConfig target_config;
    std::vector<CompileNode> nodes;
};

struct TargetBuildGraph {
    ProjectFile project;
    ProjectTarget target;
    CompileGraph compile_graph;
    std::vector<TargetBuildGraph> linked_targets;
};

std::filesystem::path DefaultProjectPath() {
    return std::filesystem::current_path() / "build.toml";
}

std::string BootstrapTargetIdentity() {
    const auto config = mc::codegen_llvm::BootstrapTargetConfig();
    return config.triple;
}

bool IsSupportedMode(std::string_view mode) {
    return mode == "debug" || mode == "release" || mode == "checked";
}

bool IsSupportedEnv(std::string_view env) {
    return env == "hosted";
}

bool IsExecutableTargetKind(std::string_view kind) {
    return kind == "exe" || kind == "test";
}

bool IsStaticLibraryTargetKind(std::string_view kind) {
    return kind == "staticlib";
}

std::string HexU64(std::uint64_t value) {
    static constexpr char kDigits[] = "0123456789abcdef";
    std::string text(16, '0');
    for (int index = 15; index >= 0; --index) {
        text[static_cast<std::size_t>(index)] = kDigits[value & 0xfu];
        value >>= 4u;
    }
    return text;
}

std::string HashText(std::string_view text) {
    // FNV-1a 64-bit is deterministic and cheap for trusted local build caching.
    // It is not collision-resistant against adversarial inputs.
    std::uint64_t hash = 1469598103934665603ull;
    for (const unsigned char byte : text) {
        hash ^= static_cast<std::uint64_t>(byte);
        hash *= 1099511628211ull;
    }
    return HexU64(hash);
}

std::filesystem::path ComputeModuleStatePath(const std::filesystem::path& source_path,
                                             const std::filesystem::path& build_dir) {
    return build_dir / "state" / (support::SanitizeArtifactStem(source_path) + ".state.txt");
}

std::filesystem::path ComputeRuntimeObjectPath(const std::filesystem::path& entry_source_path,
                                               const std::filesystem::path& build_dir) {
    const auto build_targets = support::ComputeBuildArtifactTargets(entry_source_path, build_dir);
    return build_targets.object.parent_path() / (build_targets.object.stem().generic_string() + ".runtime.o");
}

std::string FormatCommandForDisplay(const std::vector<std::string>& args) {
    std::ostringstream command;
    for (std::size_t index = 0; index < args.size(); ++index) {
        if (index > 0) {
            command << ' ';
        }
        command << '\'';
        for (const char ch : args[index]) {
            if (ch == '\'') {
                command << "'\\''";
                continue;
            }
            command << ch;
        }
        command << '\'';
    }
    return command.str();
}

enum class WaitForChildResult {
    kExited,
    kWaitError,
    kTimedOut,
};

WaitForChildResult WaitForChildProcess(const pid_t pid,
                                       const std::optional<int>& timeout_ms,
                                       int& status) {
    if (!timeout_ms.has_value() || *timeout_ms <= 0) {
        return waitpid(pid, &status, 0) == pid ? WaitForChildResult::kExited : WaitForChildResult::kWaitError;
    }

    struct WaitState {
        pid_t wait_result = -1;
        int wait_status = 0;
        bool done = false;
    };

    WaitState state;
    std::mutex mutex;
    std::condition_variable condition;
    std::thread waiter([&]() {
        int local_status = 0;
        const pid_t local_result = waitpid(pid, &local_status, 0);

        {
            std::lock_guard<std::mutex> lock(mutex);
            state.wait_result = local_result;
            state.wait_status = local_status;
            state.done = true;
        }
        condition.notify_one();
    });

    std::unique_lock<std::mutex> lock(mutex);
    const bool completed = condition.wait_for(lock,
                                              std::chrono::milliseconds(*timeout_ms),
                                              [&]() { return state.done; });
    if (!completed) {
        kill(pid, SIGKILL);
        lock.unlock();
        waiter.join();
        if (state.wait_result < 0) {
            return WaitForChildResult::kWaitError;
        }
        if (WIFSIGNALED(state.wait_status) && WTERMSIG(state.wait_status) == SIGKILL) {
            return WaitForChildResult::kTimedOut;
        }
        status = state.wait_status;
        return state.wait_result == pid ? WaitForChildResult::kExited : WaitForChildResult::kWaitError;
    }

    status = state.wait_status;
    lock.unlock();
    waiter.join();
    return state.wait_result == pid ? WaitForChildResult::kExited : WaitForChildResult::kWaitError;
}

int RunExecutableCommand(const std::filesystem::path& executable_path,
                         const std::vector<std::string>& arguments,
                         const std::optional<int>& timeout_ms = std::nullopt) {
    std::vector<std::string> command;
    command.reserve(arguments.size() + 1);
    command.push_back(executable_path.generic_string());
    command.insert(command.end(), arguments.begin(), arguments.end());

    std::vector<char*> argv;
    argv.reserve(command.size() + 1);
    for (auto& arg : command) {
        argv.push_back(arg.data());
    }
    argv.push_back(nullptr);

    const pid_t pid = fork();
    if (pid < 0) {
        return 127;
    }
    if (pid == 0) {
        execv(executable_path.c_str(), argv.data());
        _exit(127);
    }

    int status = 0;
    switch (WaitForChildProcess(pid, timeout_ms, status)) {
        case WaitForChildResult::kExited:
#ifdef WIFEXITED
            if (WIFEXITED(status)) {
                return WEXITSTATUS(status);
            }
#endif
#ifdef WIFSIGNALED
            if (WIFSIGNALED(status)) {
                return 128 + WTERMSIG(status);
            }
#endif
            return status;
        case WaitForChildResult::kWaitError:
            return 127;
        case WaitForChildResult::kTimedOut:
            return 124;
    }

    MC_UNREACHABLE("unhandled child wait result");
}

std::optional<ModuleBuildState> LoadModuleBuildState(const std::filesystem::path& path,
                                                     support::DiagnosticSink& diagnostics) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return std::nullopt;
    }

    const auto read_state_line = [&](std::string_view expected_key) -> std::optional<std::string> {
        std::string line;
        if (!std::getline(input, line)) {
            return std::nullopt;
        }

        const std::size_t tab = line.find('\t');
        if (tab == std::string::npos || std::string_view(line.data(), tab) != expected_key) {
            diagnostics.Report({
                .file_path = path,
                .span = support::kDefaultSourceSpan,
                .severity = support::DiagnosticSeverity::kError,
                .message = "invalid module build state entry",
            });
            return std::nullopt;
        }

        return line.substr(tab + 1);
    };

    const auto format_text = read_state_line("format");
    if (!format_text.has_value()) {
        return std::nullopt;
    }
    const auto parsed_format = support::ParseArrayLength(*format_text);
    if (!parsed_format.has_value()) {
        diagnostics.Report({
            .file_path = path,
            .span = support::kDefaultSourceSpan,
            .severity = support::DiagnosticSeverity::kError,
            .message = "invalid module build state format entry",
        });
        return std::nullopt;
    }
    if (static_cast<int>(*parsed_format) != kModuleBuildStateFormatVersion) {
        return std::nullopt;
    }

    auto target_identity = read_state_line("target");
    auto mode = read_state_line("mode");
    auto env = read_state_line("env");
    auto source_hash = read_state_line("source_hash");
    auto interface_hash = read_state_line("interface_hash");
    auto wrap_hosted_main_text = read_state_line("wrap_hosted_main");
    if (!target_identity.has_value() || !mode.has_value() || !env.has_value() ||
        !source_hash.has_value() || !interface_hash.has_value() || !wrap_hosted_main_text.has_value()) {
        return std::nullopt;
    }

    ModuleBuildState state {
        .target_identity = std::move(*target_identity),
        .mode = std::move(*mode),
        .env = std::move(*env),
        .source_hash = std::move(*source_hash),
        .interface_hash = std::move(*interface_hash),
        .wrap_hosted_main = *wrap_hosted_main_text == "1",
    };

    std::string line;
    while (std::getline(input, line)) {
        if (line.empty()) {
            diagnostics.Report({
                .file_path = path,
                .span = support::kDefaultSourceSpan,
                .severity = support::DiagnosticSeverity::kError,
                .message = "invalid module build state entry",
            });
            return std::nullopt;
        }

        const std::size_t tab = line.find('\t');
        if (tab == std::string::npos || std::string_view(line.data(), tab) != "import_hash") {
            diagnostics.Report({
                .file_path = path,
                .span = support::kDefaultSourceSpan,
                .severity = support::DiagnosticSeverity::kError,
                .message = "invalid module build state entry",
            });
            return std::nullopt;
        }

        const std::string value = line.substr(tab + 1);
        const std::size_t equals = value.find('=');
        if (equals == std::string::npos) {
            diagnostics.Report({
                .file_path = path,
                .span = support::kDefaultSourceSpan,
                .severity = support::DiagnosticSeverity::kError,
                .message = "invalid imported interface hash entry",
            });
            return std::nullopt;
        }
        state.imported_interface_hashes.push_back({value.substr(0, equals), value.substr(equals + 1)});
    }

    return state;
}

bool WriteModuleBuildState(const std::filesystem::path& path,
                           const ModuleBuildState& state,
                           support::DiagnosticSink& diagnostics) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        diagnostics.Report({
            .file_path = path,
            .span = support::kDefaultSourceSpan,
            .severity = support::DiagnosticSeverity::kError,
            .message = "unable to write module build state",
        });
        return false;
    }

    output << "format\t" << kModuleBuildStateFormatVersion << '\n';
    output << "target\t" << state.target_identity << '\n';
    output << "mode\t" << state.mode << '\n';
    output << "env\t" << state.env << '\n';
    output << "source_hash\t" << state.source_hash << '\n';
    output << "interface_hash\t" << state.interface_hash << '\n';
    output << "wrap_hosted_main\t" << (state.wrap_hosted_main ? "1" : "0") << '\n';
    for (const auto& [import_name, hash] : state.imported_interface_hashes) {
        output << "import_hash\t" << import_name << '=' << hash << '\n';
    }
    return static_cast<bool>(output);
}

bool ShouldReuseModuleObject(const ModuleBuildState& state,
                             const ModuleBuildState& current,
                             const std::filesystem::path& object_path,
                             const std::filesystem::path& mci_path) {
    if (!std::filesystem::exists(object_path) || !std::filesystem::exists(mci_path)) {
        return false;
    }
    if (state.target_identity != current.target_identity || state.mode != current.mode ||
        state.env != current.env || state.source_hash != current.source_hash ||
        state.interface_hash != current.interface_hash ||
        state.wrap_hosted_main != current.wrap_hosted_main) {
        return false;
    }
    return state.imported_interface_hashes == current.imported_interface_hashes;
}

std::optional<ProjectFile> LoadSelectedProject(const CommandOptions& options,
                                               support::DiagnosticSink& diagnostics) {
    const std::filesystem::path project_path = options.project_path.has_value()
                                                   ? std::filesystem::absolute(*options.project_path).lexically_normal()
                                                   : std::filesystem::absolute(DefaultProjectPath()).lexically_normal();
    return LoadProjectFile(project_path, diagnostics);
}

bool SupportsBootstrapTarget(const ProjectTarget& target,
                            const ProjectFile& project,
                            support::DiagnosticSink& diagnostics) {
    if (target.target.empty()) {
        return true;
    }

    const auto config = mc::codegen_llvm::BootstrapTargetConfig();
    if (target.target == config.triple || target.target == config.target_family) {
        return true;
    }

    diagnostics.Report({
        .file_path = project.path,
        .span = support::kDefaultSourceSpan,
        .severity = support::DiagnosticSeverity::kError,
        .message = "target '" + target.name + "' requests unsupported bootstrap target: " + target.target,
    });
    return false;
}

bool DiscoverModuleGraphRecursive(const std::string& module_name,
                                  const std::filesystem::path& source_path,
                                  const std::vector<std::filesystem::path>& import_roots,
                                  const std::unordered_set<std::string>& external_module_paths,
                                  support::DiagnosticSink& diagnostics,
                                  std::unordered_set<std::string>& visiting,
                                  std::unordered_set<std::string>& visited,
                                  std::vector<CompileNode>& nodes) {
    const std::filesystem::path normalized_path = std::filesystem::absolute(source_path).lexically_normal();
    const std::string path_key = normalized_path.generic_string();
    if (visited.contains(path_key)) {
        return true;
    }
    if (!visiting.insert(path_key).second) {
        diagnostics.Report({
            .file_path = normalized_path,
            .span = support::kDefaultSourceSpan,
            .severity = support::DiagnosticSeverity::kError,
            .message = "import cycle detected involving module: " + module_name,
        });
        return false;
    }

    auto parse_result = ParseSourcePath(normalized_path, diagnostics);
    if (!parse_result.has_value()) {
        visiting.erase(path_key);
        return false;
    }

    CompileNode node {
        .module_name = module_name,
        .source_path = normalized_path,
        .parse_result = std::move(*parse_result),
    };

    for (const auto& import_decl : node.parse_result.source_file->imports) {
        const auto import_path = mc::support::ResolveImportPathFromRoots(normalized_path,
                                                                         import_decl.module_name,
                                                                         import_roots,
                                                                         diagnostics,
                                                                         import_decl.span);
        if (!import_path.has_value()) {
            visiting.erase(path_key);
            return false;
        }
        node.imports.push_back({import_decl.module_name, *import_path});
        const std::string import_key = std::filesystem::absolute(*import_path).lexically_normal().generic_string();
        if (external_module_paths.contains(import_key)) {
            continue;
        }
        if (!DiscoverModuleGraphRecursive(import_decl.module_name,
                                          *import_path,
                                          import_roots,
                                          external_module_paths,
                                          diagnostics,
                                          visiting,
                                          visited,
                                          nodes)) {
            visiting.erase(path_key);
            return false;
        }
    }

    visited.insert(path_key);
    visiting.erase(path_key);
    nodes.push_back(std::move(node));
    return true;
}

std::optional<CompileGraph> DiscoverModuleGraph(
    const std::filesystem::path& entry_source_path,
    const std::vector<std::filesystem::path>& import_roots,
    const std::unordered_set<std::string>& external_module_paths,
    const std::filesystem::path& build_dir,
    const mc::codegen_llvm::TargetConfig& target_config,
    std::string mode,
    std::string env,
    bool wrap_entry_main,
    support::DiagnosticSink& diagnostics) {
    const std::filesystem::path normalized_entry =
        std::filesystem::absolute(entry_source_path).lexically_normal();
    CompileGraph graph {
        .entry_source_path = normalized_entry,
        .import_roots = import_roots,
        .build_dir = build_dir,
        .mode = std::move(mode),
        .env = std::move(env),
        .wrap_entry_main = wrap_entry_main,
        .target_config = target_config,
    };
    std::unordered_set<std::string> visiting;
    std::unordered_set<std::string> visited;
    if (!DiscoverModuleGraphRecursive(normalized_entry.stem().string(),
                                      normalized_entry,
                                      import_roots,
                                      external_module_paths,
                                      diagnostics,
                                      visiting,
                                      visited,
                                      graph.nodes)) {
        return std::nullopt;
    }
    return graph;
}

void AssertCompileGraphTopologicalOrder(const CompileGraph& graph) {
#ifdef NDEBUG
    (void)graph;
#else
    std::unordered_map<std::string, std::size_t> node_indices;
    node_indices.reserve(graph.nodes.size());
    for (std::size_t index = 0; index < graph.nodes.size(); ++index) {
        node_indices.emplace(graph.nodes[index].source_path.generic_string(), index);
    }

    for (std::size_t index = 0; index < graph.nodes.size(); ++index) {
        for (const auto& [import_name, import_path] : graph.nodes[index].imports) {
            const std::string import_key = std::filesystem::absolute(import_path).lexically_normal().generic_string();
            const auto found = node_indices.find(import_key);
            if (found == node_indices.end()) {
                continue;
            }
            (void)import_name;
            assert(found->second < index && "compile graph nodes must be topologically ordered");
        }
    }
#endif
}

void CollectOwnedSourcePathsForLink(const TargetBuildGraph& graph,
                                    std::string_view owner_name,
                                    std::unordered_set<std::string>& owned_paths,
                                    std::unordered_map<std::string, std::string>& owners,
                                    const ProjectTarget& consumer_target,
                                    const ProjectFile& project,
                                    support::DiagnosticSink& diagnostics) {
    for (const auto& node : graph.compile_graph.nodes) {
        const std::string key = node.source_path.generic_string();
        const auto [it, inserted] = owners.emplace(key, std::string(owner_name));
        if (!inserted && it->second != owner_name) {
            diagnostics.Report({
                .file_path = project.path,
                .span = support::kDefaultSourceSpan,
                .severity = support::DiagnosticSeverity::kError,
                .message = "target '" + consumer_target.name + "' links static libraries '" + it->second +
                           "' and '" + std::string(owner_name) + "' that both provide module source: " + key,
            });
            continue;
        }
        owned_paths.insert(key);
    }

    for (const auto& linked_graph : graph.linked_targets) {
        CollectOwnedSourcePathsForLink(linked_graph,
                                       owner_name,
                                       owned_paths,
                                       owners,
                                       consumer_target,
                                       project,
                                       diagnostics);
    }
}

mc::codegen_llvm::TargetConfig ResolveProjectTargetConfig(const ProjectFile& project,
                                                          const ProjectTarget& target,
                                                          support::DiagnosticSink& diagnostics) {
    mc::codegen_llvm::TargetConfig target_config = mc::codegen_llvm::BootstrapTargetConfig();
    target_config.hosted = target.env == "hosted";
    if (!target.target.empty() && target.target == target_config.triple) {
        target_config.triple = target.target;
    }
    (void)project;
    (void)diagnostics;
    return target_config;
}

std::optional<TargetBuildGraph> BuildResolvedTargetGraph(const ProjectFile& project,
                                                         const ProjectTarget& target,
                                                         const std::vector<std::filesystem::path>& cli_import_roots,
                                                         support::DiagnosticSink& diagnostics,
                                                         std::unordered_set<std::string>& visiting_targets) {
    if (!SupportsBootstrapTarget(target, project, diagnostics)) {
        return std::nullopt;
    }
    if (!IsExecutableTargetKind(target.kind) && !IsStaticLibraryTargetKind(target.kind)) {
        diagnostics.Report({
            .file_path = project.path,
            .span = support::kDefaultSourceSpan,
            .severity = support::DiagnosticSeverity::kError,
            .message = "target '" + target.name + "' uses unsupported bootstrap build target kind: " + target.kind,
        });
        return std::nullopt;
    }

    if (!visiting_targets.insert(target.name).second) {
        diagnostics.Report({
            .file_path = project.path,
            .span = support::kDefaultSourceSpan,
            .severity = support::DiagnosticSeverity::kError,
            .message = "target link cycle detected involving target '" + target.name + "'",
        });
        return std::nullopt;
    }

    std::vector<TargetBuildGraph> linked_targets;
    linked_targets.reserve(target.links.size());
    for (const auto& link_name : target.links) {
        const auto linked_it = project.targets.find(link_name);
        if (linked_it == project.targets.end()) {
            diagnostics.Report({
                .file_path = project.path,
                .span = support::kDefaultSourceSpan,
                .severity = support::DiagnosticSeverity::kError,
                .message = "target '" + target.name + "' links unsupported external library or unknown target: " + link_name,
            });
            continue;
        }

        ProjectTarget linked_target = linked_it->second;
        linked_target.mode = target.mode;
        linked_target.env = target.env;
        if (!IsStaticLibraryTargetKind(linked_target.kind)) {
            diagnostics.Report({
                .file_path = project.path,
                .span = support::kDefaultSourceSpan,
                .severity = support::DiagnosticSeverity::kError,
                .message = "target '" + target.name + "' can only link static libraries, but '" + linked_target.name +
                           "' has kind '" + linked_target.kind + "'",
            });
            continue;
        }

        auto linked_graph = BuildResolvedTargetGraph(project, linked_target, cli_import_roots, diagnostics, visiting_targets);
        if (!linked_graph.has_value()) {
            visiting_targets.erase(target.name);
            return std::nullopt;
        }
        linked_targets.push_back(std::move(*linked_graph));
    }
    visiting_targets.erase(target.name);

    if (diagnostics.HasErrors()) {
        return std::nullopt;
    }

    std::unordered_set<std::string> external_module_paths;
    std::unordered_map<std::string, std::string> external_owners;
    for (const auto& linked_graph : linked_targets) {
        CollectOwnedSourcePathsForLink(linked_graph,
                                       linked_graph.target.name,
                                       external_module_paths,
                                       external_owners,
                                       target,
                                       project,
                                       diagnostics);
    }
    if (diagnostics.HasErrors()) {
        return std::nullopt;
    }

    const auto import_roots = ComputeProjectImportRoots(project, target, cli_import_roots);
    auto compile_graph = DiscoverModuleGraph(target.root,
                                             import_roots,
                                             external_module_paths,
                                             project.build_dir,
                                             ResolveProjectTargetConfig(project, target, diagnostics),
                                             target.mode,
                                             target.env,
                                             IsExecutableTargetKind(target.kind),
                                             diagnostics);
    if (!compile_graph.has_value()) {
        return std::nullopt;
    }

    return TargetBuildGraph {
        .project = project,
        .target = target,
        .compile_graph = std::move(*compile_graph),
        .linked_targets = std::move(linked_targets),
    };
}

std::optional<TargetBuildGraph> BuildTargetGraph(const CommandOptions& options,
                                                 support::DiagnosticSink& diagnostics) {
    auto project = LoadSelectedProject(options, diagnostics);
    if (!project.has_value()) {
        return std::nullopt;
    }

    const ProjectTarget* target = SelectProjectTarget(*project, options.target_name, diagnostics);
    if (target == nullptr || diagnostics.HasErrors()) {
        return std::nullopt;
    }
    const std::string mode = options.mode_override.value_or(target->mode);
    const std::string env = options.env_override.value_or(target->env);

    if (!IsSupportedMode(mode)) {
        diagnostics.Report({
            .file_path = project->path,
            .span = support::kDefaultSourceSpan,
            .severity = support::DiagnosticSeverity::kError,
            .message = "unsupported build mode for target build: " + mode,
        });
        return std::nullopt;
    }
    if (!IsSupportedEnv(env)) {
        diagnostics.Report({
            .file_path = project->path,
            .span = support::kDefaultSourceSpan,
            .severity = support::DiagnosticSeverity::kError,
            .message = "unsupported environment for target build: " + env,
        });
        return std::nullopt;
    }

    const std::filesystem::path build_dir = options.build_dir_explicit
        ? std::filesystem::absolute(options.build_dir).lexically_normal()
        : std::filesystem::absolute(project->build_dir / mode).lexically_normal();

    ProjectFile resolved_project = *project;
    resolved_project.build_dir = build_dir;
    ProjectTarget resolved_target = *target;
    resolved_target.mode = mode;
    resolved_target.env = env;
    std::unordered_set<std::string> visiting_targets;
    return BuildResolvedTargetGraph(resolved_project,
                                    resolved_target,
                                    options.import_roots,
                                    diagnostics,
                                    visiting_targets);
}

mc::mci::InterfaceArtifact MakeModuleInterfaceArtifact(const std::filesystem::path& source_path,
                                                       const ast::SourceFile& source_file,
                                                       const sema::Module& checked_module,
                                                       std::string target_identity) {
    return {
        .target_identity = std::move(target_identity),
        .module_name = source_path.stem().string(),
        .source_path = source_path,
        .module = sema::BuildExportedModuleSurface(checked_module, source_file),
    };
}

bool WriteModuleInterface(const std::filesystem::path& source_path,
                          const mc::mci::InterfaceArtifact& artifact,
                          const std::filesystem::path& build_dir,
                          support::DiagnosticSink& diagnostics) {
    const auto dump_targets = support::ComputeDumpTargets(source_path, build_dir);
    return mc::mci::WriteInterfaceArtifact(dump_targets.mci, artifact, diagnostics);
}

std::optional<ImportedInterfaceData> LoadImportedInterfaces(
    const CompileNode& node,
    std::string_view target_identity,
    const std::filesystem::path& build_dir,
    support::DiagnosticSink& diagnostics) {
    ImportedInterfaceData imported;
    for (const auto& [import_name, import_path] : node.imports) {
        const auto dump_targets = support::ComputeDumpTargets(import_path, build_dir);
        const auto artifact = mc::mci::LoadInterfaceArtifact(dump_targets.mci, diagnostics);
        if (!artifact.has_value()) {
            return std::nullopt;
        }
        if (!mc::mci::ValidateInterfaceArtifactMetadata(*artifact,
                                                        dump_targets.mci,
                                                        target_identity,
                                                        import_path.stem().string(),
                                                        import_path,
                                                        diagnostics)) {
            return std::nullopt;
        }
        imported.modules.emplace(import_name, mc::sema::RewriteImportedModuleSurfaceTypes(artifact->module, import_name));
        imported.interface_hashes.push_back({import_name, artifact->interface_hash});
    }
    return imported;
}

void AddImportedExternDeclarations(mc::mir::Module& module,
                                   const std::unordered_map<std::string, mc::sema::Module>& imported_modules);

mc::mir::TypeDecl ConvertImportedTypeDecl(const mc::sema::TypeDeclSummary& type_decl);

void NamespaceImportedBuildUnit(mc::mir::Module& module,
                                std::string_view module_name);

std::optional<std::vector<BuildUnit>> CompileModuleGraph(CompileGraph& graph,
                                                        support::DiagnosticSink& diagnostics,
                                                        bool emit_objects) {
    // Per module, the driver runs a fixed dependency-ordered pipeline:
    // 1. Load imported interfaces.
    // 2. Recompute the current incremental-build state.
    // 3. Run sema.
    // 4. Write the interface artifact when it changed.
    // 5. Lower and validate MIR.
    // 6. Optionally build the object file and persist state.
    // The bootstrap driver assumes a single mc invocation owns a build
    // directory; state and interface files are rewritten without file locks.
    AssertCompileGraphTopologicalOrder(graph);

    std::vector<BuildUnit> units;
    units.reserve(graph.nodes.size());

    for (auto& node : graph.nodes) {
        sema::CheckOptions sema_options;
        const auto imported_data = LoadImportedInterfaces(node, graph.target_config.triple, graph.build_dir, diagnostics);
        if (!imported_data.has_value()) {
            return std::nullopt;
        }
        if (!imported_data->modules.empty()) {
            sema_options.imported_modules = &imported_data->modules;
        }

        const auto source_text = ReadSourceText(node.source_path, diagnostics);
        if (!source_text.has_value()) {
            return std::nullopt;
        }

        auto sema_result = mc::sema::CheckProgram(*node.parse_result.source_file,
                                                  node.source_path,
                                                  sema_options,
                                                  diagnostics);
        if (!sema_result.ok) {
            return std::nullopt;
        }

        mc::mci::InterfaceArtifact interface_artifact = MakeModuleInterfaceArtifact(node.source_path,
                                                                                    *node.parse_result.source_file,
                                                                                    *sema_result.module,
                                                                                    graph.target_config.triple);
        const std::string interface_hash = mc::mci::ComputeInterfaceHash(interface_artifact);
        const auto dump_targets = support::ComputeDumpTargets(node.source_path, graph.build_dir);
        const auto build_targets = support::ComputeBuildArtifactTargets(node.source_path, graph.build_dir);
        const auto state_path = ComputeModuleStatePath(node.source_path, graph.build_dir);

        ModuleBuildState current_state {
            .target_identity = graph.target_config.triple,
            .mode = graph.mode,
            .env = graph.env,
            .source_hash = HashText(*source_text),
            .interface_hash = interface_hash,
            .wrap_hosted_main = graph.wrap_entry_main && node.source_path == graph.entry_source_path,
            .imported_interface_hashes = imported_data->interface_hashes,
        };
        std::sort(current_state.imported_interface_hashes.begin(), current_state.imported_interface_hashes.end());

        const auto previous_state = LoadModuleBuildState(state_path, diagnostics);
        const bool reuse_object = emit_objects && previous_state.has_value() &&
                                  ShouldReuseModuleObject(*previous_state, current_state, build_targets.object, dump_targets.mci);

        bool interface_written = false;
        std::optional<mc::mci::InterfaceArtifact> previous_interface;
        if (std::filesystem::exists(dump_targets.mci)) {
            previous_interface = mc::mci::LoadInterfaceArtifact(dump_targets.mci, diagnostics);
            if (!previous_interface.has_value()) {
                return std::nullopt;
            }
            if (!mc::mci::ValidateInterfaceArtifactMetadata(*previous_interface,
                                                            dump_targets.mci,
                                                            graph.target_config.triple,
                                                            node.source_path.stem().string(),
                                                            node.source_path,
                                                            diagnostics)) {
                return std::nullopt;
            }
        }
        const bool interface_changed = !previous_interface.has_value() || previous_interface->interface_hash != interface_hash;
        if (interface_changed) {
            if (!WriteModuleInterface(node.source_path, interface_artifact, graph.build_dir, diagnostics)) {
                return std::nullopt;
            }
            interface_written = true;
        }

        auto mir_result = mc::mir::LowerSourceFile(*node.parse_result.source_file,
                                                   *sema_result.module,
                                                   node.source_path,
                                                   diagnostics);
        if (emit_objects && mir_result.ok && !imported_data->modules.empty()) {
            AddImportedExternDeclarations(*mir_result.module, imported_data->modules);
        }
        if (mir_result.ok && (node.source_path != graph.entry_source_path || !graph.wrap_entry_main)) {
            NamespaceImportedBuildUnit(*mir_result.module, node.source_path.stem().string());
        }
        if (!mir_result.ok || !mc::mir::ValidateModule(*mir_result.module, node.source_path, diagnostics)) {
            return std::nullopt;
        }

        bool reused_object = false;
        if (emit_objects) {
            if (!reuse_object) {
                const auto object_result = mc::codegen_llvm::BuildObjectFile(
                    *mir_result.module,
                    node.source_path,
                    {
                        .target = graph.target_config,
                        .artifacts = {
                            .llvm_ir_path = build_targets.llvm_ir,
                            .object_path = build_targets.object,
                        },
                        .wrap_hosted_main = current_state.wrap_hosted_main,
                    },
                    diagnostics);
                if (!object_result.ok) {
                    return std::nullopt;
                }
                if (!WriteModuleBuildState(state_path, current_state, diagnostics)) {
                    return std::nullopt;
                }
            } else {
                reused_object = true;
                if (!interface_written && !std::filesystem::exists(dump_targets.mci)) {
                    if (!WriteModuleInterface(node.source_path, interface_artifact, graph.build_dir, diagnostics)) {
                        return std::nullopt;
                    }
                }
            }
        } else if (!interface_changed && !std::filesystem::exists(dump_targets.mci)) {
            if (!WriteModuleInterface(node.source_path, interface_artifact, graph.build_dir, diagnostics)) {
                return std::nullopt;
            }
        }

        BuildUnit unit {
            .source_path = node.source_path,
            .parse_result = std::move(node.parse_result),
            .sema_result = std::move(sema_result),
            .mir_result = std::move(mir_result),
            .llvm_ir_path = build_targets.llvm_ir,
            .object_path = build_targets.object,
            .interface_hash = interface_hash,
            .reused_object = reused_object,
        };
        units.push_back(std::move(unit));
    }

    return units;
}

bool PrepareLinkedTargetInterfaces(std::vector<TargetBuildGraph>& linked_targets,
                                   support::DiagnosticSink& diagnostics) {
    for (auto& linked_graph : linked_targets) {
        if (!PrepareLinkedTargetInterfaces(linked_graph.linked_targets, diagnostics)) {
            return false;
        }
        if (!CompileModuleGraph(linked_graph.compile_graph, diagnostics, false).has_value()) {
            return false;
        }
    }
    return true;
}

std::string QualifyImportedSymbol(std::string_view module_name,
                                  std::string_view symbol_name) {
    return std::string(module_name) + "." + std::string(symbol_name);
}

void AddImportedExternDeclarations(mc::mir::Module& module,
                                   const std::unordered_map<std::string, mc::sema::Module>& imported_modules) {
    std::unordered_set<std::string> existing_types;
    existing_types.reserve(module.type_decls.size());
    for (const auto& type_decl : module.type_decls) {
        existing_types.insert(type_decl.name);
    }

    std::unordered_set<std::string> existing_functions;
    existing_functions.reserve(module.functions.size());
    for (const auto& function : module.functions) {
        existing_functions.insert(function.name);
    }

    std::unordered_set<std::string> existing_globals;
    for (const auto& global : module.globals) {
        for (const auto& name : global.names) {
            existing_globals.insert(name);
        }
    }

    std::vector<std::pair<std::string, const mc::sema::Module*>> ordered_imports;
    ordered_imports.reserve(imported_modules.size());
    for (const auto& [module_name, imported_module] : imported_modules) {
        ordered_imports.push_back({module_name, &imported_module});
    }
    std::sort(ordered_imports.begin(), ordered_imports.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.first < rhs.first;
    });

    for (const auto& [module_name, imported_module_ptr] : ordered_imports) {
        const auto& imported_module = *imported_module_ptr;
        for (const auto& type_decl : imported_module.type_decls) {
            if (!existing_types.insert(type_decl.name).second) {
                continue;
            }
            module.type_decls.push_back(ConvertImportedTypeDecl(type_decl));
        }

        for (const auto& function : imported_module.functions) {
            const std::string qualified_name = QualifyImportedSymbol(module_name, function.name);
            if (!existing_functions.insert(qualified_name).second) {
                continue;
            }

            mc::mir::Function extern_function;
            extern_function.name = qualified_name;
            extern_function.is_extern = true;
            extern_function.extern_abi = function.extern_abi;
            extern_function.type_params = function.type_params;
            for (std::size_t index = 0; index < function.param_types.size(); ++index) {
                extern_function.locals.push_back({
                    .name = "arg" + std::to_string(index),
                    .type = function.param_types[index],
                    .is_parameter = true,
                    .is_mutable = false,
                });
            }
            extern_function.return_types = function.return_types;
            module.functions.push_back(std::move(extern_function));
        }

        for (const auto& global : imported_module.globals) {
            mc::mir::GlobalDecl extern_global;
            extern_global.is_const = global.is_const;
            extern_global.is_extern = true;
            extern_global.type = global.type;
            for (const auto& name : global.names) {
                const std::string qualified_name = QualifyImportedSymbol(module_name, name);
                if (!existing_globals.insert(qualified_name).second) {
                    continue;
                }
                extern_global.names.push_back(qualified_name);
            }
            if (!extern_global.names.empty()) {
                module.globals.push_back(std::move(extern_global));
            }
        }
    }
}

mc::mir::TypeDecl ConvertImportedTypeDecl(const mc::sema::TypeDeclSummary& type_decl) {
    mc::mir::TypeDecl mir_type_decl;
    switch (type_decl.kind) {
        case mc::ast::Decl::Kind::kStruct:
            mir_type_decl.kind = mc::mir::TypeDecl::Kind::kStruct;
            break;
        case mc::ast::Decl::Kind::kEnum:
            mir_type_decl.kind = mc::mir::TypeDecl::Kind::kEnum;
            break;
        case mc::ast::Decl::Kind::kDistinct:
            mir_type_decl.kind = mc::mir::TypeDecl::Kind::kDistinct;
            break;
        case mc::ast::Decl::Kind::kTypeAlias:
            mir_type_decl.kind = mc::mir::TypeDecl::Kind::kAlias;
            break;
        case mc::ast::Decl::Kind::kFunc:
        case mc::ast::Decl::Kind::kExternFunc:
        case mc::ast::Decl::Kind::kConst:
        case mc::ast::Decl::Kind::kVar:
            break;
    }
    mir_type_decl.name = type_decl.name;
    mir_type_decl.type_params = type_decl.type_params;
    mir_type_decl.is_packed = type_decl.is_packed;
    mir_type_decl.fields = type_decl.fields;
    mir_type_decl.aliased_type = type_decl.aliased_type;
    for (const auto& variant : type_decl.variants) {
        mc::mir::VariantDecl mir_variant;
        mir_variant.name = variant.name;
        mir_variant.payload_fields = variant.payload_fields;
        mir_type_decl.variants.push_back(std::move(mir_variant));
    }
    return mir_type_decl;
}

mc::sema::Type RewriteImportedTypeNames(mc::sema::Type type,
                                        const std::unordered_map<std::string, std::string>& renamed_types) {
    for (auto& subtype : type.subtypes) {
        subtype = RewriteImportedTypeNames(std::move(subtype), renamed_types);
    }

    if (type.kind != mc::sema::Type::Kind::kNamed) {
        return type;
    }

    const auto found = renamed_types.find(type.name);
    if (found != renamed_types.end()) {
        type.name = found->second;
    }
    return type;
}

void RewriteImportedTypeDecl(mc::mir::TypeDecl& type_decl,
                             const std::unordered_map<std::string, std::string>& renamed_types) {
    type_decl.aliased_type = RewriteImportedTypeNames(std::move(type_decl.aliased_type), renamed_types);
    for (auto& field : type_decl.fields) {
        field.second = RewriteImportedTypeNames(std::move(field.second), renamed_types);
    }
    for (auto& variant : type_decl.variants) {
        for (auto& payload_field : variant.payload_fields) {
            payload_field.second = RewriteImportedTypeNames(std::move(payload_field.second), renamed_types);
        }
    }
}

void RewriteImportedSymbolReference(const std::unordered_map<std::string, std::string>& renamed_functions,
                                    const std::unordered_map<std::string, std::string>& renamed_globals,
                                    mc::mir::Instruction::TargetKind target_kind,
                                    std::string& value) {
    if (value.empty()) {
        return;
    }

    if (target_kind == mc::mir::Instruction::TargetKind::kFunction) {
        const auto found = renamed_functions.find(value);
        if (found != renamed_functions.end()) {
            value = found->second;
        }
        return;
    }

    if (target_kind == mc::mir::Instruction::TargetKind::kGlobal) {
        const auto found = renamed_globals.find(value);
        if (found != renamed_globals.end()) {
            value = found->second;
        }
    }
}

// NamespaceImportedBuildUnit qualifies every non-extern symbol in `module`
// with `module_name` as a prefix (e.g. "Foo" becomes "mymod.Foo").
//
// INVARIANT: all name-bearing fields in mir::Instruction must be rewritten
// here.  When adding a new Instruction field that carries a symbol name
// (function reference, type reference, global reference), add a rewrite case
// in this function.  Forgetting to do so will produce broken cross-module
// references in the merged module that are invisible until link time.
void NamespaceImportedBuildUnit(mc::mir::Module& module,
                                std::string_view module_name) {
    std::unordered_map<std::string, std::string> renamed_types;
    std::unordered_map<std::string, std::string> renamed_functions;
    std::unordered_map<std::string, std::string> renamed_globals;

    for (auto& type_decl : module.type_decls) {
        if (type_decl.name.find('.') != std::string::npos) {
            continue;
        }
        const std::string qualified_name = QualifyImportedSymbol(module_name, type_decl.name);
        renamed_types.emplace(type_decl.name, qualified_name);
        type_decl.name = qualified_name;
    }

    for (auto& type_decl : module.type_decls) {
        RewriteImportedTypeDecl(type_decl, renamed_types);
    }

    for (auto& function : module.functions) {
        if (function.is_extern) {
        } else {
            const std::string qualified_name = QualifyImportedSymbol(module_name, function.name);
            renamed_functions.emplace(function.name, qualified_name);
            function.name = qualified_name;
        }

        for (auto& local : function.locals) {
            local.type = RewriteImportedTypeNames(std::move(local.type), renamed_types);
        }
        for (auto& return_type : function.return_types) {
            return_type = RewriteImportedTypeNames(std::move(return_type), renamed_types);
        }
    }

    for (auto& global : module.globals) {
        global.type = RewriteImportedTypeNames(std::move(global.type), renamed_types);
        for (auto& name : global.names) {
            const std::string qualified_name = QualifyImportedSymbol(module_name, name);
            renamed_globals.emplace(name, qualified_name);
            name = qualified_name;
        }
    }

    for (auto& function : module.functions) {
        for (auto& block : function.blocks) {
            for (auto& instruction : block.instructions) {
                instruction.type = RewriteImportedTypeNames(std::move(instruction.type), renamed_types);
                instruction.target_base_type = RewriteImportedTypeNames(std::move(instruction.target_base_type), renamed_types);
                for (auto& target_aux_type : instruction.target_aux_types) {
                    target_aux_type = RewriteImportedTypeNames(std::move(target_aux_type), renamed_types);
                }
                RewriteImportedSymbolReference(renamed_functions, renamed_globals, instruction.target_kind, instruction.target);
                RewriteImportedSymbolReference(renamed_functions, renamed_globals, instruction.target_kind, instruction.target_name);
                RewriteImportedSymbolReference(renamed_functions, renamed_globals, instruction.target_kind, instruction.target_display);
            }
        }
    }
}


std::unique_ptr<mc::mir::Module> MergeBuildUnits(const std::vector<BuildUnit>& units,
                                                 const mc::mir::Module& entry_module,
                                                 const std::filesystem::path& entry_source_path) {
    auto merged = std::make_unique<mc::mir::Module>();
    std::unordered_set<std::string> seen_imports;
    std::unordered_set<std::string> seen_types;
    std::unordered_set<std::string> seen_functions;
    std::unordered_set<std::string> seen_globals;
    std::unordered_set<std::string> defined_by_deps;
    for (const auto& unit : units) {
        if (unit.source_path == std::filesystem::absolute(entry_source_path).lexically_normal()) {
            continue;
        }
        mc::mir::Module& unit_module = *unit.mir_result.module;
        for (const auto& import_name : unit_module.imports) {
            if (seen_imports.insert(import_name).second) {
                merged->imports.push_back(import_name);
            }
        }
        for (const auto& type_decl : unit_module.type_decls) {
            if (!seen_types.insert(type_decl.name).second) {
                continue;
            }
            merged->type_decls.push_back(type_decl);
        }
        for (const auto& global : unit_module.globals) {
            mc::mir::GlobalDecl merged_global = global;
            merged_global.names.clear();
            for (const auto& name : global.names) {
                if (seen_globals.insert(name).second) {
                    merged_global.names.push_back(name);
                }
            }
            if (!merged_global.names.empty()) {
                merged->globals.push_back(std::move(merged_global));
            }
        }
        for (const auto& function : unit_module.functions) {
            if (function.is_extern && !seen_functions.insert(function.name).second) {
                continue;
            }
            if (!function.is_extern) {
                defined_by_deps.insert(function.name);
                seen_functions.insert(function.name);
            }
            merged->functions.push_back(function);
        }
    }

    for (const auto& import_name : entry_module.imports) {
        if (seen_imports.insert(import_name).second) {
            merged->imports.push_back(import_name);
        }
    }
    for (const auto& type_decl : entry_module.type_decls) {
        if (!seen_types.insert(type_decl.name).second) {
            continue;
        }
        merged->type_decls.push_back(type_decl);
    }
    for (const auto& global : entry_module.globals) {
        mc::mir::GlobalDecl merged_global = global;
        merged_global.names.clear();
        for (const auto& name : global.names) {
            if (seen_globals.insert(name).second) {
                merged_global.names.push_back(name);
            }
        }
        if (!merged_global.names.empty()) {
            merged->globals.push_back(std::move(merged_global));
        }
    }
    for (const auto& function : entry_module.functions) {
        if (function.is_extern) {
            if (!seen_functions.insert(function.name).second) {
                continue;
            }
            merged->functions.push_back(function);
            continue;
        }
        if (!defined_by_deps.count(function.name)) {
            seen_functions.insert(function.name);
            merged->functions.push_back(function);
        }
    }
    return merged;
}

std::optional<BuildUnit> CompileToMir(const CommandOptions& options,
                                      support::DiagnosticSink& diagnostics,
                                      bool include_imports_for_build) {
    const auto entry_path = std::filesystem::absolute(options.source_path).lexically_normal();
    const auto import_roots = ComputeEffectiveImportRoots(entry_path, options.import_roots);
    const auto bootstrap_config = mc::codegen_llvm::BootstrapTargetConfig();
    auto graph = DiscoverModuleGraph(
        entry_path,
        import_roots,
        {},
        options.build_dir,
        bootstrap_config,
        "debug",
        "hosted",
        true,
        diagnostics);
    if (!graph.has_value()) {
        return std::nullopt;
    }

    auto units = CompileModuleGraph(*graph, diagnostics, false);
    if (!units.has_value() || units->empty()) {
        return std::nullopt;
    }

    BuildUnit& entry_unit = units->back();
    if (include_imports_for_build && units->size() > 1) {
        auto merged_module = MergeBuildUnits(*units, *entry_unit.mir_result.module, entry_unit.source_path);
        if (!mc::mir::ValidateModule(*merged_module, entry_unit.source_path, diagnostics)) {
            return std::nullopt;
        }
        entry_unit.mir_result.module = std::move(merged_module);
    }

    return BuildUnit {
        .source_path = entry_unit.source_path,
        .parse_result = std::move(entry_unit.parse_result),
        .sema_result = std::move(entry_unit.sema_result),
        .mir_result = std::move(entry_unit.mir_result),
    };
}

bool WriteTextArtifact(const std::filesystem::path& path,
                       std::string_view contents,
                       std::string_view description,
                       support::DiagnosticSink& diagnostics) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output << contents;
    if (output) {
        return true;
    }

    diagnostics.Report({
        .file_path = path,
        .span = support::kDefaultSourceSpan,
        .severity = support::DiagnosticSeverity::kError,
        .message = "unable to write " + std::string(description),
    });
    return false;
}

bool IsOutputOlderThan(const std::filesystem::path& output_path,
                      const std::filesystem::path& input_path) {
    if (!std::filesystem::exists(output_path) || !std::filesystem::exists(input_path)) {
        return true;
    }
    return std::filesystem::last_write_time(output_path) < std::filesystem::last_write_time(input_path);
}

bool ShouldRelinkProjectExecutable(const std::filesystem::path& executable_path,
                                   const std::filesystem::path& runtime_object_path,
                                   const std::optional<std::filesystem::path>& runtime_source_path,
                                   const std::vector<std::filesystem::path>& library_paths,
                                   const std::vector<BuildUnit>& units) {
    if (!std::filesystem::exists(executable_path) || !std::filesystem::exists(runtime_object_path)) {
        return true;
    }
    if (runtime_source_path.has_value() && IsOutputOlderThan(runtime_object_path, *runtime_source_path)) {
        return true;
    }
    if (IsOutputOlderThan(executable_path, runtime_object_path)) {
        return true;
    }
    for (const auto& unit : units) {
        if (!unit.reused_object || IsOutputOlderThan(executable_path, unit.object_path)) {
            return true;
        }
    }
    for (const auto& library_path : library_paths) {
        if (IsOutputOlderThan(executable_path, library_path)) {
            return true;
        }
    }
    return false;
}

bool ShouldArchiveProjectStaticLibrary(const std::filesystem::path& archive_path,
                                      const std::vector<BuildUnit>& units) {
    if (!std::filesystem::exists(archive_path)) {
        return true;
    }
    for (const auto& unit : units) {
        if (!unit.reused_object || IsOutputOlderThan(archive_path, unit.object_path)) {
            return true;
        }
    }
    return false;
}

struct ProjectBuildResult {
    std::vector<BuildUnit> units;
    std::optional<std::filesystem::path> executable_path;
    std::optional<std::filesystem::path> static_library_path;
    std::vector<std::filesystem::path> link_library_paths;
};

struct OrdinaryTestCase {
    std::filesystem::path source_path;
    std::string module_name;
    std::string function_name;
};

enum class CompilerRegressionKind {
    kCheckPass,
    kCheckFail,
    kRunOutput,
    kMir,
};

struct CompilerRegressionCase {
    CompilerRegressionKind kind = CompilerRegressionKind::kCheckPass;
    std::filesystem::path source_path;
    std::filesystem::path expectation_path;
    int expected_exit_code = 0;
};

struct DiscoveredTargetTests {
    std::vector<std::filesystem::path> roots;
    std::vector<OrdinaryTestCase> ordinary_tests;
    std::vector<CompilerRegressionCase> regression_cases;
};

struct CapturedCommandResult {
    bool exited = false;
    int exit_code = -1;
    bool signaled = false;
    int signal_number = -1;
    bool timed_out = false;
    std::string output;
};

CapturedCommandResult RunExecutableCapture(const std::filesystem::path& executable_path,
                                          const std::vector<std::string>& arguments,
                                          const std::filesystem::path& output_path,
                                          const std::optional<int>& timeout_ms);

std::optional<ProjectBuildResult> BuildProjectTarget(TargetBuildGraph& graph,
                                                     support::DiagnosticSink& diagnostics);

int RunBuild(const CommandOptions& options);

std::string TrimWhitespace(std::string_view text) {
    std::size_t begin = 0;
    while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
        ++begin;
    }

    std::size_t end = text.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
        --end;
    }

    return std::string(text.substr(begin, end - begin));
}

std::string StripComment(std::string_view text) {
    const std::size_t comment = text.find('#');
    if (comment == std::string_view::npos) {
        return std::string(text);
    }
    return std::string(text.substr(0, comment));
}

bool HasSuffix(std::string_view text,
               std::string_view suffix) {
    return text.size() >= suffix.size() && text.substr(text.size() - suffix.size()) == suffix;
}

bool IsBootstrapTestReturnType(const ast::TypeExpr& type_expr) {
    if (type_expr.kind == ast::TypeExpr::Kind::kPointer) {
        return true;
    }
    return type_expr.kind == ast::TypeExpr::Kind::kNamed &&
           (type_expr.name == "Error" || type_expr.name == "testing.Error");
}

std::vector<std::filesystem::path> ComputeTargetTestRoots(const ProjectFile& project,
                                                          const ProjectTarget& target) {
    if (!target.tests.roots.empty()) {
        return target.tests.roots;
    }
    return {project.root_dir / "tests"};
}

void SortPaths(std::vector<std::filesystem::path>& paths) {
    std::sort(paths.begin(), paths.end(), [](const std::filesystem::path& left, const std::filesystem::path& right) {
        return left.generic_string() < right.generic_string();
    });
}

std::vector<std::filesystem::path> DiscoverFilesRecursive(const std::vector<std::filesystem::path>& roots,
                                                          std::string_view suffix) {
    std::vector<std::filesystem::path> files;
    for (const auto& root : roots) {
        if (!std::filesystem::exists(root)) {
            continue;
        }
        for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            const std::string path = entry.path().generic_string();
            if (HasSuffix(path, suffix)) {
                files.push_back(entry.path());
            }
        }
    }
    SortPaths(files);
    return files;
}

bool DiscoverOrdinaryTestsInFile(const std::filesystem::path& source_path,
                                 support::DiagnosticSink& diagnostics,
                                 std::vector<OrdinaryTestCase>& ordinary_tests) {
    auto parse_result = ParseSourcePath(source_path, diagnostics);
    if (!parse_result.has_value()) {
        return false;
    }

    const std::string module_name = source_path.stem().string();
    for (const auto& decl : parse_result->source_file->decls) {
        if (decl.kind != ast::Decl::Kind::kFunc || !decl.name.starts_with("test_")) {
            continue;
        }
        if (!decl.type_params.empty() || !decl.params.empty() || decl.return_types.size() != 1 ||
            decl.return_types.front() == nullptr || !IsBootstrapTestReturnType(*decl.return_types.front())) {
            diagnostics.Report({
                .file_path = source_path,
                .span = decl.span,
                .severity = support::DiagnosticSeverity::kError,
                .message = "test function '" + decl.name + "' must use the bootstrap test signature func() *T or func() Error",
            });
            return false;
        }
        ordinary_tests.push_back({
            .source_path = source_path,
            .module_name = module_name,
            .function_name = decl.name,
        });
    }
    return true;
}

std::optional<CompilerRegressionKind> ParseRegressionKind(std::string_view token) {
    if (token == "check-pass") {
        return CompilerRegressionKind::kCheckPass;
    }
    if (token == "check-fail") {
        return CompilerRegressionKind::kCheckFail;
    }
    if (token == "run-output") {
        return CompilerRegressionKind::kRunOutput;
    }
    if (token == "mir") {
        return CompilerRegressionKind::kMir;
    }
    return std::nullopt;
}

bool ParseCompilerManifest(const std::filesystem::path& manifest_path,
                          support::DiagnosticSink& diagnostics,
                          std::vector<CompilerRegressionCase>& regression_cases) {
    std::ifstream input(manifest_path, std::ios::binary);
    if (!input) {
        diagnostics.Report({
            .file_path = manifest_path,
            .span = support::kDefaultSourceSpan,
            .severity = support::DiagnosticSeverity::kError,
            .message = "unable to read compiler regression manifest",
        });
        return false;
    }

    std::string line;
    std::size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        const std::string trimmed = TrimWhitespace(StripComment(line));
        if (trimmed.empty()) {
            continue;
        }

        std::istringstream tokens(trimmed);
        std::string kind_text;
        std::string source_text;
        if (!(tokens >> kind_text >> source_text)) {
            diagnostics.Report({
                .file_path = manifest_path,
                .span = {{line_number, 1}, {line_number, 1}},
                .severity = support::DiagnosticSeverity::kError,
                .message = "invalid compiler regression manifest entry",
            });
            return false;
        }

        const auto kind = ParseRegressionKind(kind_text);
        if (!kind.has_value()) {
            diagnostics.Report({
                .file_path = manifest_path,
                .span = {{line_number, 1}, {line_number, 1}},
                .severity = support::DiagnosticSeverity::kError,
                .message = "unknown compiler regression kind: " + kind_text,
            });
            return false;
        }

        CompilerRegressionCase regression_case {
            .kind = *kind,
            .source_path = std::filesystem::absolute(manifest_path.parent_path() / source_text).lexically_normal(),
        };

        switch (*kind) {
            case CompilerRegressionKind::kCheckPass:
                break;

            case CompilerRegressionKind::kCheckFail:
            case CompilerRegressionKind::kMir: {
                std::string expectation_text;
                if (!(tokens >> expectation_text)) {
                    diagnostics.Report({
                        .file_path = manifest_path,
                        .span = {{line_number, 1}, {line_number, 1}},
                        .severity = support::DiagnosticSeverity::kError,
                        .message = "compiler regression entry is missing an expectation path",
                    });
                    return false;
                }
                regression_case.expectation_path = std::filesystem::absolute(manifest_path.parent_path() / expectation_text).lexically_normal();
                break;
            }

            case CompilerRegressionKind::kRunOutput: {
                std::string expectation_text;
                if (!(tokens >> regression_case.expected_exit_code >> expectation_text)) {
                    diagnostics.Report({
                        .file_path = manifest_path,
                        .span = {{line_number, 1}, {line_number, 1}},
                        .severity = support::DiagnosticSeverity::kError,
                        .message = "run-output regression entry must include exit code and stdout expectation path",
                    });
                    return false;
                }
                regression_case.expectation_path = std::filesystem::absolute(manifest_path.parent_path() / expectation_text).lexically_normal();
                break;
            }
        }

        regression_cases.push_back(std::move(regression_case));
    }

    return true;
}

std::optional<DiscoveredTargetTests> DiscoverTargetTests(const ProjectFile& project,
                                                        const ProjectTarget& target,
                                                        support::DiagnosticSink& diagnostics) {
    DiscoveredTargetTests discovered;
    discovered.roots = ComputeTargetTestRoots(project, target);
    SortPaths(discovered.roots);

    std::unordered_set<std::string> seen_modules;
    for (const auto& test_file : DiscoverFilesRecursive(discovered.roots, "_test.mc")) {
        const std::string module_name = test_file.stem().string();
        if (!seen_modules.insert(module_name).second) {
            diagnostics.Report({
                .file_path = test_file,
                .span = support::kDefaultSourceSpan,
                .severity = support::DiagnosticSeverity::kError,
                .message = "duplicate ordinary test module name discovered: " + module_name,
            });
            return std::nullopt;
        }
        if (!DiscoverOrdinaryTestsInFile(test_file, diagnostics, discovered.ordinary_tests)) {
            return std::nullopt;
        }
    }

    const auto manifests = DiscoverFilesRecursive(discovered.roots, "compiler_manifest.txt");
    for (const auto& manifest_path : manifests) {
        if (!ParseCompilerManifest(manifest_path, diagnostics, discovered.regression_cases)) {
            return std::nullopt;
        }
    }

    std::sort(discovered.ordinary_tests.begin(), discovered.ordinary_tests.end(), [](const OrdinaryTestCase& left, const OrdinaryTestCase& right) {
        if (left.source_path != right.source_path) {
            return left.source_path.generic_string() < right.source_path.generic_string();
        }
        return left.function_name < right.function_name;
    });
    std::sort(discovered.regression_cases.begin(), discovered.regression_cases.end(), [](const CompilerRegressionCase& left, const CompilerRegressionCase& right) {
        if (left.kind != right.kind) {
            return static_cast<int>(left.kind) < static_cast<int>(right.kind);
        }
        return left.source_path.generic_string() < right.source_path.generic_string();
    });
    return discovered;
}

std::vector<std::string> ReadExpectedLines(const std::filesystem::path& path,
                                           support::DiagnosticSink& diagnostics) {
    const auto text = ReadSourceText(path, diagnostics);
    if (!text.has_value()) {
        return {};
    }

    std::vector<std::string> lines;
    std::istringstream input(*text);
    std::string line;
    while (std::getline(input, line)) {
        const std::string trimmed = TrimWhitespace(line);
        if (!trimmed.empty()) {
            lines.push_back(trimmed);
        }
    }
    return lines;
}

std::vector<std::filesystem::path> BuildTestImportRoots(const ProjectFile& project,
                                                        const ProjectTarget& target,
                                                        const std::vector<std::filesystem::path>& cli_import_roots,
                                                        const std::vector<std::filesystem::path>& test_roots,
                                                        const std::filesystem::path& generated_root) {
    std::vector<std::filesystem::path> roots;
    std::unordered_set<std::string> seen;
    const auto append = [&](const std::filesystem::path& root) {
        const std::filesystem::path normalized = std::filesystem::absolute(root).lexically_normal();
        if (seen.insert(normalized.generic_string()).second) {
            roots.push_back(normalized);
        }
    };

    append(generated_root);
    for (const auto& root : test_roots) {
        append(root);
    }
    for (const auto& root : ComputeProjectImportRoots(project, target, cli_import_roots)) {
        append(root);
    }
    if (const auto repo_root = DiscoverRepositoryRoot(generated_root); repo_root.has_value()) {
        const std::filesystem::path stdlib_root = *repo_root / "stdlib";
        if (std::filesystem::exists(stdlib_root)) {
            append(stdlib_root);
        }
    }
    return roots;
}

std::string BuildOrdinaryRunnerSource(const std::vector<OrdinaryTestCase>& ordinary_tests) {
    std::ostringstream source;
    source << "import io\n";
    for (const auto& ordinary_test : ordinary_tests) {
        source << "import " << ordinary_test.module_name << "\n";
    }
    source << "\n";
    source << "func main() i32 {\n";
    source << "    failures: i32 = 0\n";
    for (const auto& ordinary_test : ordinary_tests) {
        const std::string test_name = ordinary_test.module_name + "." + ordinary_test.function_name;
        source << "    if " << ordinary_test.module_name << "." << ordinary_test.function_name << "() == nil {\n";
        source << "        if io.write_line(\"PASS " << test_name << "\") != 0 {\n";
        source << "            return 1\n";
        source << "        }\n";
        source << "    } else {\n";
        source << "        if io.write_line(\"FAIL " << test_name << "\") != 0 {\n";
        source << "            return 1\n";
        source << "        }\n";
        source << "        failures = failures + 1\n";
        source << "    }\n";
    }
    for (std::size_t failures = 0; failures <= ordinary_tests.size(); ++failures) {
        source << "    if failures == " << failures << " {\n";
        source << "        if io.write_line(\"" << ordinary_tests.size() << " tests, " << (ordinary_tests.size() - failures)
               << " passed, " << failures << " failed\") != 0 {\n";
        source << "            return 1\n";
        source << "        }\n";
        source << "        return " << (failures == 0 ? 0 : 1) << "\n";
        source << "    }\n";
    }
    source << "    return 1\n";
    source << "}\n";
    return source.str();
}

bool RunOrdinaryTestsForTarget(const ProjectFile& project,
                               const ProjectTarget& target,
                               const CommandOptions& options,
                               const std::vector<std::filesystem::path>& test_roots,
                               const std::vector<OrdinaryTestCase>& ordinary_tests,
                               const std::filesystem::path& build_dir) {
    const std::filesystem::path generated_root = build_dir / "generated_tests" / target.name;
    const std::filesystem::path runner_path = generated_root / "__mc_test_runner.mc";
    const std::string test_mode = options.mode_override.value_or(target.tests.mode);

    std::cout << "ordinary tests target " << target.name << ": " << ordinary_tests.size()
              << " cases, mode=" << test_mode;
    if (target.tests.timeout_ms.has_value()) {
        std::cout << ", timeout=" << *target.tests.timeout_ms << " ms";
    }
    std::cout << '\n';

    support::DiagnosticSink diagnostics;
    if (!WriteTextArtifact(runner_path, BuildOrdinaryRunnerSource(ordinary_tests), "test runner source", diagnostics)) {
        std::cerr << diagnostics.Render() << '\n';
        return false;
    }

    ProjectFile runner_project = project;
    runner_project.build_dir = build_dir;

    ProjectTarget runner_target;
    runner_target.name = target.name + "-tests";
    runner_target.kind = "exe";
    runner_target.root = runner_path;
    runner_target.mode = test_mode;
    runner_target.env = options.env_override.value_or(target.env);
    runner_target.target = target.target;
    runner_target.links = target.links;
    runner_target.module_search_paths = BuildTestImportRoots(project, target, options.import_roots, test_roots, generated_root);
    runner_target.runtime_startup = "default";

    std::unordered_set<std::string> visiting_targets;
    auto runner_graph = BuildResolvedTargetGraph(runner_project,
                                                 runner_target,
                                                 {},
                                                 diagnostics,
                                                 visiting_targets);
    if (!runner_graph.has_value()) {
        std::cerr << diagnostics.Render() << '\n';
        return false;
    }

    auto build_result = BuildProjectTarget(*runner_graph, diagnostics);
    if (!build_result.has_value() || !build_result->executable_path.has_value()) {
        std::cerr << diagnostics.Render() << '\n';
        return false;
    }

    const auto output_path = generated_root / "runner.stdout.txt";
    const auto run_result = RunExecutableCapture(*build_result->executable_path, {}, output_path, target.tests.timeout_ms);
    if (!run_result.output.empty()) {
        std::cout << run_result.output;
        if (run_result.output.back() != '\n') {
            std::cout << '\n';
        }
    }
    if (run_result.timed_out) {
        std::cout << "TIMEOUT ordinary tests for target " << target.name << " after " << target.tests.timeout_ms.value_or(0) << " ms" << '\n';
        std::cout << "FAIL ordinary tests for target " << target.name << " (timeout)" << '\n';
        return false;
    }

    const bool ok = run_result.exited && run_result.exit_code == 0;
    std::cout << (ok ? "PASS ordinary tests for target " : "FAIL ordinary tests for target ") << target.name
              << " (" << ordinary_tests.size() << " cases)" << '\n';
    return ok;
}

CapturedCommandResult RunExecutableCapture(const std::filesystem::path& executable_path,
                                          const std::vector<std::string>& arguments,
                                          const std::filesystem::path& output_path,
                                          const std::optional<int>& timeout_ms = std::nullopt) {
    std::filesystem::create_directories(output_path.parent_path());
    std::vector<std::string> command;
    command.reserve(arguments.size() + 1);
    command.push_back(executable_path.generic_string());
    command.insert(command.end(), arguments.begin(), arguments.end());

    std::vector<char*> argv;
    argv.reserve(command.size() + 1);
    for (auto& arg : command) {
        argv.push_back(arg.data());
    }
    argv.push_back(nullptr);

    CapturedCommandResult result;

    const pid_t pid = fork();
    if (pid < 0) {
        result.exited = true;
        result.exit_code = 127;
        return result;
    }
    if (pid == 0) {
        const int fd = ::open(output_path.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0644);
        if (fd < 0) {
            _exit(127);
        }
        if (dup2(fd, STDOUT_FILENO) < 0 || dup2(fd, STDERR_FILENO) < 0) {
            close(fd);
            _exit(127);
        }
        close(fd);
        execv(executable_path.c_str(), argv.data());
        _exit(127);
    }

    int status = 0;
    switch (WaitForChildProcess(pid, timeout_ms, status)) {
        case WaitForChildResult::kExited:
#ifdef WIFEXITED
            if (WIFEXITED(status)) {
                result.exited = true;
                result.exit_code = WEXITSTATUS(status);
            }
#endif
#ifdef WIFSIGNALED
            if (WIFSIGNALED(status)) {
                result.signaled = true;
                result.signal_number = WTERMSIG(status);
            }
#endif
            break;
        case WaitForChildResult::kWaitError:
            result.exited = true;
            result.exit_code = 127;
            break;
        case WaitForChildResult::kTimedOut:
            result.timed_out = true;
            result.signaled = true;
            result.signal_number = SIGKILL;
            break;
    }

    support::DiagnosticSink diagnostics;
    result.output = ReadSourceText(output_path, diagnostics).value_or("");
    return result;
}

bool EvaluateCheckPassCase(const CompilerRegressionCase& regression_case,
                           const std::vector<std::filesystem::path>& import_roots,
                           const std::filesystem::path& build_dir,
                           std::string& failure_detail) {
    support::DiagnosticSink diagnostics;
    CommandOptions options;
    options.source_path = regression_case.source_path;
    options.build_dir = build_dir;
    options.build_dir_explicit = true;
    options.import_roots = import_roots;
    if (!CompileToMir(options, diagnostics, false).has_value()) {
        failure_detail = diagnostics.Render();
        return false;
    }
    return true;
}

bool EvaluateCheckFailCase(const CompilerRegressionCase& regression_case,
                           const std::vector<std::filesystem::path>& import_roots,
                           const std::filesystem::path& build_dir,
                           std::string& failure_detail) {
    support::DiagnosticSink diagnostics;
    CommandOptions options;
    options.source_path = regression_case.source_path;
    options.build_dir = build_dir;
    options.build_dir_explicit = true;
    options.import_roots = import_roots;
    if (CompileToMir(options, diagnostics, false).has_value()) {
        failure_detail = "compiler regression should fail semantic checking";
        return false;
    }

    const auto expected_lines = ReadExpectedLines(regression_case.expectation_path, diagnostics);
    const std::string rendered = diagnostics.Render();
    for (const auto& expected_line : expected_lines) {
        if (rendered.find(expected_line) == std::string::npos) {
            failure_detail = "missing expected diagnostic substring: " + expected_line + "\n" + rendered;
            return false;
        }
    }
    return true;
}

bool EvaluateMirCase(const CompilerRegressionCase& regression_case,
                     const std::vector<std::filesystem::path>& import_roots,
                     const std::filesystem::path& build_dir,
                     std::string& failure_detail) {
    support::DiagnosticSink diagnostics;
    CommandOptions options;
    options.source_path = regression_case.source_path;
    options.build_dir = build_dir;
    options.build_dir_explicit = true;
    options.import_roots = import_roots;
    const auto checked = CompileToMir(options, diagnostics, false);
    if (!checked.has_value()) {
        failure_detail = diagnostics.Render();
        return false;
    }

    const auto expected = ReadSourceText(regression_case.expectation_path, diagnostics);
    if (!expected.has_value()) {
        failure_detail = diagnostics.Render();
        return false;
    }
    const std::string actual = mc::mir::DumpModule(*checked->mir_result.module);
    if (actual != *expected) {
        failure_detail = "expected MIR fixture mismatch";
        return false;
    }
    return true;
}

bool EvaluateRunOutputCase(const CompilerRegressionCase& regression_case,
                           const std::vector<std::filesystem::path>& import_roots,
                           const std::filesystem::path& build_dir,
                           const std::optional<int>& timeout_ms,
                           std::string& failure_detail) {
    support::DiagnosticSink diagnostics;
    CommandOptions options;
    options.source_path = regression_case.source_path;
    options.build_dir = build_dir;
    options.build_dir_explicit = true;
    options.import_roots = import_roots;
    const auto checked = CompileToMir(options, diagnostics, true);
    if (!checked.has_value()) {
        failure_detail = diagnostics.Render();
        return false;
    }

    const auto build_targets = support::ComputeBuildArtifactTargets(regression_case.source_path, build_dir);
    const auto runtime_source_path = DiscoverHostedRuntimeSupportSource(regression_case.source_path);
    const auto build_result = mc::codegen_llvm::BuildExecutable(
        *checked->mir_result.module,
        regression_case.source_path,
        {
            .target = mc::codegen_llvm::BootstrapTargetConfig(),
            .runtime_source_path = runtime_source_path,
            .artifacts = {
                .llvm_ir_path = build_targets.llvm_ir,
                .object_path = build_targets.object,
                .executable_path = build_targets.executable,
            },
        },
        diagnostics);
    if (!build_result.ok) {
        failure_detail = diagnostics.Render();
        return false;
    }

    support::DiagnosticSink output_diagnostics;
    const auto expected = ReadSourceText(regression_case.expectation_path, output_diagnostics);
    if (!expected.has_value()) {
        failure_detail = output_diagnostics.Render();
        return false;
    }
    const CapturedCommandResult run_result = RunExecutableCapture(build_targets.executable,
                                                                  {},
                                                                  build_dir / (support::SanitizeArtifactStem(regression_case.source_path) + ".stdout.txt"),
                                                                  timeout_ms);
    if (run_result.timed_out) {
        failure_detail = "test executable timed out after " + std::to_string(timeout_ms.value_or(0)) + " ms";
        return false;
    }
    if (!run_result.exited || run_result.exit_code != regression_case.expected_exit_code || run_result.output != *expected) {
        failure_detail = "expected exit code " + std::to_string(regression_case.expected_exit_code) + " and stdout '" + *expected +
                         "', got exit=" + std::to_string(run_result.exit_code) + " stdout='" + run_result.output + "'";
        return false;
    }
    return true;
}

bool RunCompilerRegressionCases(const std::vector<CompilerRegressionCase>& regression_cases,
                                const std::vector<std::filesystem::path>& import_roots,
                                const std::filesystem::path& build_dir,
                                const std::optional<int>& timeout_ms) {
    bool ok = true;
    for (std::size_t index = 0; index < regression_cases.size(); ++index) {
        const auto& regression_case = regression_cases[index];
        std::string failure_detail;
        const std::filesystem::path case_build_dir = build_dir / "compiler_regressions" / std::to_string(index);
        std::string label;
        bool case_ok = false;
        switch (regression_case.kind) {
            case CompilerRegressionKind::kCheckPass:
                label = "check-pass ";
                case_ok = EvaluateCheckPassCase(regression_case, import_roots, case_build_dir, failure_detail);
                break;
            case CompilerRegressionKind::kCheckFail:
                label = "check-fail ";
                case_ok = EvaluateCheckFailCase(regression_case, import_roots, case_build_dir, failure_detail);
                break;
            case CompilerRegressionKind::kRunOutput:
                label = "run-output ";
                case_ok = EvaluateRunOutputCase(regression_case, import_roots, case_build_dir, timeout_ms, failure_detail);
                break;
            case CompilerRegressionKind::kMir:
                label = "mir ";
                case_ok = EvaluateMirCase(regression_case, import_roots, case_build_dir, failure_detail);
                break;
        }

        std::cout << (case_ok ? "PASS " : "FAIL ") << label << regression_case.source_path.generic_string() << '\n';
        if (!case_ok) {
            ok = false;
            if (!failure_detail.empty()) {
                std::cout << failure_detail << '\n';
            }
        }
    }
    return ok;
}

std::vector<const ProjectTarget*> SelectTestTargets(const ProjectFile& project,
                                                    const std::optional<std::string>& explicit_target,
                                                    support::DiagnosticSink& diagnostics) {
    std::vector<const ProjectTarget*> targets;
    if (explicit_target.has_value()) {
        const ProjectTarget* target = SelectProjectTarget(project, explicit_target, diagnostics);
        if (target != nullptr) {
            if (!target->tests.enabled) {
                diagnostics.Report({
                    .file_path = project.path,
                    .span = support::kDefaultSourceSpan,
                    .severity = support::DiagnosticSeverity::kError,
                    .message = "tests are not enabled for target '" + target->name +
                               "'; enabled test targets: " + JoinNames(CollectEnabledTestTargetNames(project)),
                });
            } else {
                targets.push_back(target);
            }
        }
        return targets;
    }

    for (const auto& [name, target] : project.targets) {
        if (target.tests.enabled) {
            targets.push_back(&target);
        }
    }
    std::sort(targets.begin(), targets.end(), [](const ProjectTarget* left, const ProjectTarget* right) {
        return left->name < right->name;
    });
    if (targets.empty()) {
        diagnostics.Report({
            .file_path = project.path,
            .span = support::kDefaultSourceSpan,
            .severity = support::DiagnosticSeverity::kError,
            .message = "project file does not enable tests for any target",
        });
    }
    return targets;
}

void AppendUniquePaths(std::vector<std::filesystem::path>& destination,
                       const std::vector<std::filesystem::path>& paths) {
    std::unordered_set<std::string> seen;
    seen.reserve(destination.size() + paths.size());
    for (const auto& path : destination) {
        seen.insert(path.generic_string());
    }
    for (const auto& path : paths) {
        if (seen.insert(path.generic_string()).second) {
            destination.push_back(path);
        }
    }
}

std::optional<ProjectBuildResult> BuildProjectTarget(TargetBuildGraph& graph,
                                                     support::DiagnosticSink& diagnostics) {
    std::vector<std::filesystem::path> linked_library_paths;
    linked_library_paths.reserve(graph.linked_targets.size());
    for (auto& linked_graph : graph.linked_targets) {
        auto linked_result = BuildProjectTarget(linked_graph, diagnostics);
        if (!linked_result.has_value()) {
            return std::nullopt;
        }
        AppendUniquePaths(linked_library_paths, linked_result->link_library_paths);
    }

    auto units = CompileModuleGraph(graph.compile_graph, diagnostics, true);
    if (!units.has_value() || units->empty()) {
        return std::nullopt;
    }

    const BuildUnit& entry_unit = units->back();
    const auto build_targets = support::ComputeBuildArtifactTargets(entry_unit.source_path, graph.compile_graph.build_dir);

    if (IsStaticLibraryTargetKind(graph.target.kind)) {
        if (ShouldArchiveProjectStaticLibrary(build_targets.static_library, *units)) {
            std::vector<std::filesystem::path> object_paths;
            object_paths.reserve(units->size());
            for (const auto& unit : *units) {
                object_paths.push_back(unit.object_path);
            }

            const auto archive_result = mc::codegen_llvm::ArchiveStaticLibrary(entry_unit.source_path,
                                                                               {
                                                                                   .target = graph.compile_graph.target_config,
                                                                                   .object_paths = object_paths,
                                                                                   .archive_path = build_targets.static_library,
                                                                               },
                                                                               diagnostics);
            if (!archive_result.ok) {
                return std::nullopt;
            }
        }

        std::vector<std::filesystem::path> link_paths {build_targets.static_library};
        AppendUniquePaths(link_paths, linked_library_paths);
        return ProjectBuildResult {
            .units = std::move(*units),
            .static_library_path = build_targets.static_library,
            .link_library_paths = std::move(link_paths),
        };
    }

    const auto runtime_object_path = ComputeRuntimeObjectPath(entry_unit.source_path, graph.compile_graph.build_dir);
    const auto runtime_source_path = DiscoverHostedRuntimeSupportSource(entry_unit.source_path);

    if (ShouldRelinkProjectExecutable(build_targets.executable,
                                      runtime_object_path,
                                      runtime_source_path,
                                      linked_library_paths,
                                      *units)) {
        std::vector<std::filesystem::path> object_paths;
        object_paths.reserve(units->size());
        for (const auto& unit : *units) {
            object_paths.push_back(unit.object_path);
        }

        const auto link_result = mc::codegen_llvm::LinkExecutable(entry_unit.source_path,
                                                                  {
                                                                      .target = graph.compile_graph.target_config,
                                                                      .object_paths = object_paths,
                                                                      .library_paths = linked_library_paths,
                                                                      .runtime_source_path = runtime_source_path,
                                                                      .runtime_object_path = runtime_object_path,
                                                                      .executable_path = build_targets.executable,
                                                                  },
                                                                  diagnostics);
        if (!link_result.ok) {
            return std::nullopt;
        }
    }

    return ProjectBuildResult {
        .units = std::move(*units),
        .executable_path = build_targets.executable,
        .link_library_paths = std::move(linked_library_paths),
    };
}

int RunCheck(const CommandOptions& options) {
    support::DiagnosticSink diagnostics;
    if (ClassifyInvocation(options) == InvocationKind::kDirectSource) {
        const auto checked = CompileToMir(options, diagnostics, false);
        if (!checked.has_value()) {
            std::cerr << diagnostics.Render() << '\n';
            return 1;
        }

        const auto targets = support::ComputeDumpTargets(checked->source_path, options.build_dir);

        if (options.dump_ast &&
            !WriteTextArtifact(targets.ast,
                               mc::ast::DumpSourceFile(*checked->parse_result.source_file),
                               "AST dump",
                               diagnostics)) {
            std::cerr << diagnostics.Render() << '\n';
            return 1;
        }

        if (options.dump_mir &&
            !WriteTextArtifact(targets.mir,
                               mc::mir::DumpModule(*checked->mir_result.module),
                               "MIR dump",
                               diagnostics)) {
            std::cerr << diagnostics.Render() << '\n';
            return 1;
        }

        if (options.dump_backend) {
            const auto backend_result = mc::codegen_llvm::LowerModule(*checked->mir_result.module,
                                                                      checked->source_path,
                                                                      {.target = mc::codegen_llvm::BootstrapTargetConfig()},
                                                                      diagnostics);
            if (!backend_result.ok ||
                !WriteTextArtifact(targets.backend,
                                   mc::codegen_llvm::DumpModule(*backend_result.module),
                                   "backend dump",
                                   diagnostics)) {
                std::cerr << diagnostics.Render() << '\n';
                return 1;
            }
        }

        std::cout << "checked " << checked->source_path.generic_string()
                  << " (bootstrap phase 3 sema, phase 4 MIR)" << '\n';

        if (options.emit_dump_paths) {
            PrintDumpTargets(targets, std::cout);
        }

        return 0;
    }

    auto graph = BuildTargetGraph(options, diagnostics);
    if (!graph.has_value()) {
        std::cerr << diagnostics.Render() << '\n';
        return 1;
    }
    if (!PrepareLinkedTargetInterfaces(graph->linked_targets, diagnostics)) {
        std::cerr << diagnostics.Render() << '\n';
        return 1;
    }
    auto units = CompileModuleGraph(graph->compile_graph, diagnostics, false);
    if (!units.has_value() || units->empty()) {
        std::cerr << diagnostics.Render() << '\n';
        return 1;
    }

    const BuildUnit& entry_unit = units->back();
    const auto dump_targets = support::ComputeDumpTargets(entry_unit.source_path, graph->compile_graph.build_dir);

    if (options.dump_ast &&
        !WriteTextArtifact(dump_targets.ast,
                           mc::ast::DumpSourceFile(*entry_unit.parse_result.source_file),
                           "AST dump",
                           diagnostics)) {
        std::cerr << diagnostics.Render() << '\n';
        return 1;
    }

    if (options.dump_mir &&
        !WriteTextArtifact(dump_targets.mir,
                           mc::mir::DumpModule(*entry_unit.mir_result.module),
                           "MIR dump",
                           diagnostics)) {
        std::cerr << diagnostics.Render() << '\n';
        return 1;
    }

    if (options.dump_backend) {
        const auto backend_result = mc::codegen_llvm::LowerModule(*entry_unit.mir_result.module,
                                      entry_unit.source_path,
                                      {.target = graph->compile_graph.target_config},
                                                                  diagnostics);
        if (!backend_result.ok ||
            !WriteTextArtifact(dump_targets.backend,
                               mc::codegen_llvm::DumpModule(*backend_result.module),
                               "backend dump",
                               diagnostics)) {
            std::cerr << diagnostics.Render() << '\n';
            return 1;
        }
    }

    std::cout << "checked target " << graph->target.name << " from " << graph->project.path.generic_string()
              << " (bootstrap phase 7 project graph)" << '\n';
    if (options.emit_dump_paths) {
        PrintDumpTargets(dump_targets, std::cout);
    }
    return 0;
}

int RunBuild(const CommandOptions& options) {
    support::DiagnosticSink diagnostics;
    if (ClassifyInvocation(options) == InvocationKind::kDirectSource) {
        const auto checked = CompileToMir(options, diagnostics, true);
        if (!checked.has_value()) {
            std::cerr << diagnostics.Render() << '\n';
            return 1;
        }

        const auto dump_targets = support::ComputeDumpTargets(checked->source_path, options.build_dir);
        const auto build_targets = support::ComputeBuildArtifactTargets(checked->source_path, options.build_dir);

        if (options.dump_ast &&
            !WriteTextArtifact(dump_targets.ast,
                               mc::ast::DumpSourceFile(*checked->parse_result.source_file),
                               "AST dump",
                               diagnostics)) {
            std::cerr << diagnostics.Render() << '\n';
            return 1;
        }

        if (options.dump_mir &&
            !WriteTextArtifact(dump_targets.mir,
                               mc::mir::DumpModule(*checked->mir_result.module),
                               "MIR dump",
                               diagnostics)) {
            std::cerr << diagnostics.Render() << '\n';
            return 1;
        }

        if (options.dump_backend) {
            const auto backend_result = mc::codegen_llvm::LowerModule(*checked->mir_result.module,
                                                                      checked->source_path,
                                                                      {.target = mc::codegen_llvm::BootstrapTargetConfig()},
                                                                      diagnostics);
            if (!backend_result.ok ||
                !WriteTextArtifact(dump_targets.backend,
                                   mc::codegen_llvm::DumpModule(*backend_result.module),
                                   "backend dump",
                                   diagnostics)) {
                std::cerr << diagnostics.Render() << '\n';
                return 1;
            }
        }

        const auto build_result = mc::codegen_llvm::BuildExecutable(
            *checked->mir_result.module,
            checked->source_path,
            {
                .target = mc::codegen_llvm::BootstrapTargetConfig(),
                .runtime_source_path = DiscoverHostedRuntimeSupportSource(checked->source_path),
                .artifacts = {
                    .llvm_ir_path = build_targets.llvm_ir,
                    .object_path = build_targets.object,
                    .executable_path = build_targets.executable,
                },
            },
            diagnostics);
        if (!build_result.ok) {
            std::cerr << diagnostics.Render() << '\n';
            return 1;
        }

        std::cout << "built " << checked->source_path.generic_string() << " -> "
                  << build_result.artifacts.executable_path.generic_string()
                  << " (bootstrap phase 5 executable path)" << '\n';

        if (options.emit_dump_paths) {
            PrintArtifactTargets(dump_targets, build_targets, std::cout);
        }

        return 0;
    }

    auto graph = BuildTargetGraph(options, diagnostics);
    if (!graph.has_value()) {
        std::cerr << diagnostics.Render() << '\n';
        return 1;
    }
    auto build_result = BuildProjectTarget(*graph, diagnostics);
    if (!build_result.has_value() || build_result->units.empty()) {
        std::cerr << diagnostics.Render() << '\n';
        return 1;
    }

    const BuildUnit& entry_unit = build_result->units.back();
    auto merged_module = MergeBuildUnits(build_result->units, *entry_unit.mir_result.module, entry_unit.source_path);
    if (!mc::mir::ValidateModule(*merged_module, entry_unit.source_path, diagnostics)) {
        std::cerr << diagnostics.Render() << '\n';
        return 1;
    }

    const auto dump_targets = support::ComputeDumpTargets(entry_unit.source_path, graph->compile_graph.build_dir);
    const auto build_targets = support::ComputeBuildArtifactTargets(entry_unit.source_path, graph->compile_graph.build_dir);

    if (options.dump_ast &&
        !WriteTextArtifact(dump_targets.ast,
                           mc::ast::DumpSourceFile(*entry_unit.parse_result.source_file),
                           "AST dump",
                           diagnostics)) {
        std::cerr << diagnostics.Render() << '\n';
        return 1;
    }

    if (options.dump_mir &&
        !WriteTextArtifact(dump_targets.mir,
                           mc::mir::DumpModule(*merged_module),
                           "MIR dump",
                           diagnostics)) {
        std::cerr << diagnostics.Render() << '\n';
        return 1;
    }

    if (options.dump_backend) {
        const auto backend_result = mc::codegen_llvm::LowerModule(*merged_module,
                                      entry_unit.source_path,
                                      {.target = graph->compile_graph.target_config},
                                                                  diagnostics);
        if (!backend_result.ok ||
            !WriteTextArtifact(dump_targets.backend,
                               mc::codegen_llvm::DumpModule(*backend_result.module),
                               "backend dump",
                               diagnostics)) {
            std::cerr << diagnostics.Render() << '\n';
            return 1;
        }
    }

    const std::filesystem::path primary_artifact = build_result->static_library_path.has_value()
        ? *build_result->static_library_path
        : *build_result->executable_path;
    std::cout << "built target " << graph->target.name << " -> "
              << primary_artifact.generic_string()
              << (build_result->static_library_path.has_value()
                      ? " (bootstrap phase 29 static library path)"
                      : " (bootstrap phase 7 project graph)")
              << '\n';

    if (options.emit_dump_paths) {
        PrintArtifactTargets(dump_targets, build_targets, std::cout);
    }
    return 0;
}

int RunRun(const CommandOptions& options) {
    if (ClassifyInvocation(options) == InvocationKind::kDirectSource) {
        const int build_status = RunBuild(options);
        if (build_status != 0) {
            return build_status;
        }
        const auto build_targets = support::ComputeBuildArtifactTargets(options.source_path, options.build_dir);
        return RunExecutableCommand(build_targets.executable, options.run_arguments);
    }

    support::DiagnosticSink diagnostics;
    auto graph = BuildTargetGraph(options, diagnostics);
    if (!graph.has_value()) {
        std::cerr << diagnostics.Render() << '\n';
        return 1;
    }
    if (!IsExecutableTargetKind(graph->target.kind)) {
        diagnostics.Report({
            .file_path = graph->project.path,
            .span = support::kDefaultSourceSpan,
            .severity = support::DiagnosticSeverity::kError,
            .message = "target '" + graph->target.name + "' has kind '" + graph->target.kind +
                       "' and cannot be run; choose an executable target or use mc build",
        });
        std::cerr << diagnostics.Render() << '\n';
        return 1;
    }

    auto build_result = BuildProjectTarget(*graph, diagnostics);
    if (!build_result.has_value() || !build_result->executable_path.has_value()) {
        std::cerr << diagnostics.Render() << '\n';
        return 1;
    }

    return RunExecutableCommand(*build_result->executable_path, options.run_arguments);
}

int RunTest(const CommandOptions& options) {
    support::DiagnosticSink diagnostics;
    auto project = LoadSelectedProject(options, diagnostics);
    if (!project.has_value()) {
        std::cerr << diagnostics.Render() << '\n';
        return 1;
    }

    const auto targets = SelectTestTargets(*project, options.target_name, diagnostics);
    if (diagnostics.HasErrors()) {
        std::cerr << diagnostics.Render() << '\n';
        return 1;
    }

    bool ok = true;
    for (const auto* target : targets) {
        CommandOptions target_options = options;
        target_options.project_path = project->path;
        target_options.target_name = target->name;
        if (!target_options.mode_override.has_value()) {
            target_options.mode_override = target->tests.mode;
        }

        auto graph = BuildTargetGraph(target_options, diagnostics);
        if (!graph.has_value()) {
            std::cerr << diagnostics.Render() << '\n';
            return 1;
        }

        const auto discovered = DiscoverTargetTests(*project, *target, diagnostics);
        if (!discovered.has_value()) {
            std::cerr << diagnostics.Render() << '\n';
            return 1;
        }

        std::cout << "testing target " << target->name << '\n';
        bool target_ok = true;
        if (!discovered->ordinary_tests.empty()) {
            target_ok = RunOrdinaryTestsForTarget(*project,
                                                 *target,
                                                 target_options,
                                                 discovered->roots,
                                                 discovered->ordinary_tests,
                                                 graph->compile_graph.build_dir) && target_ok;
        }
        if (!discovered->regression_cases.empty()) {
            const auto import_roots = ComputeProjectImportRoots(*project, *target, options.import_roots);
            std::cout << "compiler regressions target " << target->name << ": " << discovered->regression_cases.size() << " cases";
            if (target->tests.timeout_ms.has_value()) {
                std::cout << ", timeout=" << *target->tests.timeout_ms << " ms";
            }
            std::cout << '\n';
            const bool regressions_ok = RunCompilerRegressionCases(discovered->regression_cases,
                                                                   import_roots,
                                                                   graph->compile_graph.build_dir,
                                                                   target->tests.timeout_ms);
            std::cout << (regressions_ok ? "PASS compiler regressions for target " : "FAIL compiler regressions for target ")
                      << target->name << " (" << discovered->regression_cases.size() << " cases)" << '\n';
            target_ok = regressions_ok && target_ok;
        }
        if (discovered->ordinary_tests.empty() && discovered->regression_cases.empty()) {
            std::cout << "SKIP target " << target->name << " (no tests discovered)" << '\n';
        }
        std::cout << (target_ok ? "PASS target " : "FAIL target ") << target->name << '\n';
        ok = ok && target_ok;
    }

    return ok ? 0 : 1;
}

int RunDumpPaths(const CommandOptions& options) {
    PrintArtifactTargets(support::ComputeDumpTargets(options.source_path, options.build_dir),
                         support::ComputeBuildArtifactTargets(options.source_path, options.build_dir),
                         std::cout);
    return 0;
}

}  // namespace

int Run(int argc, const char* argv[]) {
    if (argc <= 1) {
        PrintUsage(std::cout);
        return 0;
    }

    const std::string_view command = argv[1];
    if (command == "--help" || command == "-h" || command == "help") {
        PrintUsage(std::cout);
        return 0;
    }

    if (command == "check") {
        const auto options = ParseCommandOptions(argc, argv, 2, std::cerr);
        if (!options.has_value() || !ValidateInvocationForCommand(command, *options, std::cerr)) {
            PrintUsage(std::cerr);
            return 1;
        }

        return RunCheck(*options);
    }

    if (command == "build") {
        const auto options = ParseCommandOptions(argc, argv, 2, std::cerr);
        if (!options.has_value() || !ValidateInvocationForCommand(command, *options, std::cerr)) {
            PrintUsage(std::cerr);
            return 1;
        }

        return RunBuild(*options);
    }

    if (command == "run") {
        const auto options = ParseCommandOptions(argc, argv, 2, std::cerr);
        if (!options.has_value() || !ValidateInvocationForCommand(command, *options, std::cerr)) {
            PrintUsage(std::cerr);
            return 1;
        }

        return RunRun(*options);
    }

    if (command == "test") {
        const auto options = ParseCommandOptions(argc, argv, 2, std::cerr);
        if (!options.has_value() || !ValidateInvocationForCommand(command, *options, std::cerr)) {
            PrintUsage(std::cerr);
            return 1;
        }

        return RunTest(*options);
    }

    if (command == "dump-paths") {
        const auto options = ParseCommandOptions(argc, argv, 2, std::cerr);
        if (!options.has_value() || !ValidateInvocationForCommand(command, *options, std::cerr)) {
            PrintUsage(std::cerr);
            return 1;
        }

        return RunDumpPaths(*options);
    }

    std::cerr << "unknown command: " << command << '\n';
    PrintUsage(std::cerr);
    return 1;
}

}  // namespace mc::driver
