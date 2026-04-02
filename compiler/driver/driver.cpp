#include "compiler/driver/driver.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "compiler/ast/ast.h"
#include "compiler/codegen_llvm/backend.h"
#include "compiler/driver/project.h"
#include "compiler/lex/lexer.h"
#include "compiler/mci/mci.h"
#include "compiler/mir/mir.h"
#include "compiler/parse/parser.h"
#include "compiler/sema/check.h"
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

std::optional<CommandOptions> ParseCommandOptions(int argc,
                                                  const char* argv[],
                                                  int start_index,
                                                  std::ostream& errors) {
    CommandOptions options;

    int index = start_index;
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

        if (argument == "--emit-dump-paths") {
            options.emit_dump_paths = true;
            continue;
        }

        if (argument == "--dump-ast") {
            options.dump_ast = true;
            continue;
        }

        if (argument == "--dump-mir") {
            options.dump_mir = true;
            continue;
        }

        if (argument == "--dump-backend") {
            options.dump_backend = true;
            continue;
        }

        if (argument == "--build-dir") {
            if (index + 1 >= argc) {
                errors << "missing value for --build-dir\n";
                return std::nullopt;
            }

            options.build_dir = argv[++index];
            options.build_dir_explicit = true;
            continue;
        }

        if (argument == "--project") {
            if (index + 1 >= argc) {
                errors << "missing value for --project\n";
                return std::nullopt;
            }

            options.project_path = std::filesystem::path(argv[++index]);
            continue;
        }

        if (argument == "--target") {
            if (index + 1 >= argc) {
                errors << "missing value for --target\n";
                return std::nullopt;
            }

            options.target_name = std::string(argv[++index]);
            continue;
        }

        if (argument == "--mode") {
            if (index + 1 >= argc) {
                errors << "missing value for --mode\n";
                return std::nullopt;
            }

            options.mode_override = std::string(argv[++index]);
            continue;
        }

        if (argument == "--env") {
            if (index + 1 >= argc) {
                errors << "missing value for --env\n";
                return std::nullopt;
            }

            options.env_override = std::string(argv[++index]);
            continue;
        }

        if (argument == "--import-root") {
            if (index + 1 >= argc) {
                errors << "missing value for --import-root\n";
                return std::nullopt;
            }

            options.import_roots.push_back(argv[++index]);
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
}

struct CheckedProgram {
    const support::SourceFile* source_file = nullptr;
    mc::parse::ParseResult parse_result;
    mc::sema::CheckResult sema_result;
    mc::mir::LowerResult mir_result;
};

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

struct ProjectModuleNode {
    std::string module_name;
    std::filesystem::path source_path;
    mc::parse::ParseResult parse_result;
    std::vector<std::pair<std::string, std::filesystem::path>> imports;
};

struct TargetBuildGraph {
    ProjectFile project;
    ProjectTarget target;
    std::vector<std::filesystem::path> import_roots;
    std::vector<ProjectModuleNode> nodes;
    std::string mode;
    std::string env;
    std::filesystem::path build_dir;
    mc::codegen_llvm::TargetConfig target_config;
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

std::string QuoteShellArg(std::string_view argument) {
    std::string quoted;
    quoted.reserve(argument.size() + 2);
    quoted.push_back('\'');
    for (const char ch : argument) {
        if (ch == '\'') {
            quoted += "'\\''";
            continue;
        }
        quoted.push_back(ch);
    }
    quoted.push_back('\'');
    return quoted;
}

std::string JoinCommand(const std::vector<std::string>& args) {
    std::ostringstream command;
    for (std::size_t index = 0; index < args.size(); ++index) {
        if (index > 0) {
            command << ' ';
        }
        command << QuoteShellArg(args[index]);
    }
    return command.str();
}

int RunExecutableCommand(const std::filesystem::path& executable_path,
                         const std::vector<std::string>& arguments) {
    std::vector<std::string> command;
    command.reserve(arguments.size() + 1);
    command.push_back(executable_path.generic_string());
    command.insert(command.end(), arguments.begin(), arguments.end());
    const int raw_status = std::system(JoinCommand(command).c_str());
    if (raw_status == 0) {
        return 0;
    }
#ifdef WIFEXITED
    if (WIFEXITED(raw_status)) {
        return WEXITSTATUS(raw_status);
    }
#endif
#ifdef WIFSIGNALED
    if (WIFSIGNALED(raw_status)) {
        return 128 + WTERMSIG(raw_status);
    }
#endif
    return raw_status;
}

std::optional<ModuleBuildState> LoadModuleBuildState(const std::filesystem::path& path,
                                                     support::DiagnosticSink& diagnostics) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return std::nullopt;
    }

    ModuleBuildState state;
    std::string line;
    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }

        const std::size_t tab = line.find('\t');
        if (tab == std::string::npos) {
            diagnostics.Report({
                .file_path = path,
                .span = support::kDefaultSourceSpan,
                .severity = support::DiagnosticSeverity::kError,
                .message = "invalid module build state entry",
            });
            return std::nullopt;
        }

        const std::string key = line.substr(0, tab);
        const std::string value = line.substr(tab + 1);
        if (key == "target") {
            state.target_identity = value;
            continue;
        }
        if (key == "mode") {
            state.mode = value;
            continue;
        }
        if (key == "env") {
            state.env = value;
            continue;
        }
        if (key == "source_hash") {
            state.source_hash = value;
            continue;
        }
        if (key == "interface_hash") {
            state.interface_hash = value;
            continue;
        }
        if (key == "wrap_hosted_main") {
            state.wrap_hosted_main = value == "1";
            continue;
        }
        if (key == "import_hash") {
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
            continue;
        }
    }

    if (state.target_identity.empty() || state.mode.empty() || state.env.empty() || state.source_hash.empty() || state.interface_hash.empty()) {
        return std::nullopt;
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
    return std::filesystem::exists(object_path) && std::filesystem::exists(mci_path) &&
           state.target_identity == current.target_identity && state.mode == current.mode && state.env == current.env &&
        state.source_hash == current.source_hash && state.interface_hash == current.interface_hash &&
        state.wrap_hosted_main == current.wrap_hosted_main &&
           state.imported_interface_hashes == current.imported_interface_hashes;
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

bool DiscoverProjectModuleGraphRecursive(const std::string& module_name,
                                         const std::filesystem::path& source_path,
                                         const std::vector<std::filesystem::path>& import_roots,
                                         support::DiagnosticSink& diagnostics,
                                         std::unordered_set<std::string>& visiting,
                                         std::unordered_set<std::string>& visited,
                                         std::vector<ProjectModuleNode>& nodes) {
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

    ProjectModuleNode node {
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
        if (!DiscoverProjectModuleGraphRecursive(import_decl.module_name,
                                                 *import_path,
                                                 import_roots,
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
    if (!SupportsBootstrapTarget(*target, *project, diagnostics)) {
        return std::nullopt;
    }

    TargetBuildGraph graph {
        .project = *project,
        .target = *target,
        .import_roots = ComputeProjectImportRoots(*project, *target, options.import_roots),
        .mode = options.mode_override.value_or(target->mode),
        .env = options.env_override.value_or(target->env),
        .build_dir = {},
        .target_config = mc::codegen_llvm::BootstrapTargetConfig(),
    };

    if (!IsSupportedMode(graph.mode)) {
        diagnostics.Report({
            .file_path = project->path,
            .span = support::kDefaultSourceSpan,
            .severity = support::DiagnosticSeverity::kError,
            .message = "unsupported build mode for target build: " + graph.mode,
        });
        return std::nullopt;
    }
    if (!IsSupportedEnv(graph.env)) {
        diagnostics.Report({
            .file_path = project->path,
            .span = support::kDefaultSourceSpan,
            .severity = support::DiagnosticSeverity::kError,
            .message = "unsupported environment for target build: " + graph.env,
        });
        return std::nullopt;
    }

    if (options.build_dir_explicit) {
        graph.build_dir = std::filesystem::absolute(options.build_dir).lexically_normal();
    } else {
        graph.build_dir = std::filesystem::absolute(project->build_dir / graph.mode).lexically_normal();
    }
    graph.project.build_dir = graph.build_dir;
    graph.target.mode = graph.mode;
    graph.target.env = graph.env;
    graph.target_config.hosted = graph.env == "hosted";
    if (!target->target.empty() && target->target == graph.target_config.triple) {
        graph.target_config.triple = target->target;
    }

    std::unordered_set<std::string> visiting;
    std::unordered_set<std::string> visited;
    if (!DiscoverProjectModuleGraphRecursive(target->root.stem().string(),
                                             target->root,
                                             graph.import_roots,
                                             diagnostics,
                                             visiting,
                                             visited,
                                             graph.nodes)) {
        return std::nullopt;
    }

    return graph;
}

mc::mci::InterfaceArtifact MakeModuleInterfaceArtifact(const std::filesystem::path& source_path,
                                                       const ast::SourceFile& source_file,
                                                       const sema::Module& checked_module,
                                                       std::string target_identity) {
    return {
        .format_version = 1,
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
    const ProjectModuleNode& node,
    const std::filesystem::path& build_dir,
    support::DiagnosticSink& diagnostics) {
    ImportedInterfaceData imported;
    for (const auto& [import_name, import_path] : node.imports) {
        const auto dump_targets = support::ComputeDumpTargets(import_path, build_dir);
        const auto artifact = mc::mci::LoadInterfaceArtifact(dump_targets.mci, diagnostics);
        if (!artifact.has_value()) {
            return std::nullopt;
        }
        imported.modules.emplace(import_name, mc::sema::RewriteImportedModuleSurfaceTypes(artifact->module, import_name));
        imported.interface_hashes.push_back({import_name, artifact->interface_hash});
    }
    return imported;
}

void AddImportedExternDeclarations(mc::mir::Module& module,
                                   const std::unordered_map<std::string, mc::sema::Module>& imported_modules);

void NamespaceImportedBuildUnit(mc::mir::Module& module,
                                std::string_view module_name);

std::optional<std::vector<BuildUnit>> CompileTargetUnits(TargetBuildGraph& graph,
                                                         support::DiagnosticSink& diagnostics,
                                                         bool emit_objects) {
    std::vector<BuildUnit> units;
    units.reserve(graph.nodes.size());

    for (auto& node : graph.nodes) {
        sema::CheckOptions sema_options;
        const auto imported_data = LoadImportedInterfaces(node, graph.build_dir, diagnostics);
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

        const ModuleBuildState current_state {
            .target_identity = graph.target_config.triple,
            .mode = graph.mode,
            .env = graph.env,
            .source_hash = HashText(*source_text),
            .interface_hash = interface_hash,
            .wrap_hosted_main = node.source_path == std::filesystem::absolute(graph.target.root).lexically_normal(),
            .imported_interface_hashes = imported_data->interface_hashes,
        };

        const auto previous_state = LoadModuleBuildState(state_path, diagnostics);
        const bool reuse_object = emit_objects && previous_state.has_value() &&
                                  ShouldReuseModuleObject(*previous_state, current_state, build_targets.object, dump_targets.mci);

        bool interface_written = false;
        const auto previous_interface = std::filesystem::exists(dump_targets.mci)
                                            ? mc::mci::LoadInterfaceArtifact(dump_targets.mci, diagnostics)
                                            : std::optional<mc::mci::InterfaceArtifact> {};
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
        if (mir_result.ok && !imported_data->modules.empty()) {
            AddImportedExternDeclarations(*mir_result.module, imported_data->modules);
        }
        if (mir_result.ok && node.source_path != std::filesystem::absolute(graph.target.root).lexically_normal()) {
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

std::string QualifyImportedSymbol(std::string_view module_name,
                                  std::string_view symbol_name) {
    return std::string(module_name) + "." + std::string(symbol_name);
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

    for (const auto& [module_name, imported_module] : imported_modules) {
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
    }
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

bool CollectBuildUnits(const std::filesystem::path& source_path,
                       const CommandOptions& options,
                       support::DiagnosticSink& diagnostics,
                       std::unordered_set<std::string>& visiting,
                       std::unordered_map<std::string, std::size_t>& built_indices,
                       std::vector<BuildUnit>& units) {
    const std::filesystem::path normalized_path = std::filesystem::absolute(source_path).lexically_normal();
    const std::string path_key = normalized_path.generic_string();
    if (built_indices.contains(path_key)) {
        return true;
    }
    if (!visiting.insert(path_key).second) {
        diagnostics.Report({
            .file_path = normalized_path,
            .span = support::kDefaultSourceSpan,
            .severity = support::DiagnosticSeverity::kError,
            .message = "import cycle detected involving module: " + normalized_path.stem().string(),
        });
        return false;
    }

    auto parse_result = ParseSourcePath(normalized_path, diagnostics);
    if (!parse_result.has_value()) {
        visiting.erase(path_key);
        return false;
    }

    const auto effective_import_roots = ComputeEffectiveImportRoots(normalized_path, options.import_roots);
    for (const auto& import_decl : parse_result->source_file->imports) {
        const auto import_path = mc::support::ResolveImportPath(normalized_path,
                                                                import_decl.module_name,
                                                                effective_import_roots,
                                                                diagnostics,
                                                                import_decl.span);
        if (!import_path.has_value()) {
            visiting.erase(path_key);
            return false;
        }
        if (!CollectBuildUnits(*import_path, options, diagnostics, visiting, built_indices, units)) {
            visiting.erase(path_key);
            return false;
        }
    }

    auto sema_result = mc::sema::CheckProgram(*parse_result->source_file,
                                              normalized_path,
                                              BuildSemaOptions(normalized_path, options),
                                              diagnostics);
    if (!sema_result.ok) {
        visiting.erase(path_key);
        return false;
    }
    if (!WriteModuleInterface(normalized_path,
                              MakeModuleInterfaceArtifact(normalized_path,
                                                          *parse_result->source_file,
                                                          *sema_result.module,
                                                          BootstrapTargetIdentity()),
                              options.build_dir,
                              diagnostics)) {
        visiting.erase(path_key);
        return false;
    }

    auto mir_result = mc::mir::LowerSourceFile(*parse_result->source_file,
                                               *sema_result.module,
                                               normalized_path,
                                               diagnostics);
    if (!mir_result.ok || !mc::mir::ValidateModule(*mir_result.module, normalized_path, diagnostics)) {
        visiting.erase(path_key);
        return false;
    }

    built_indices.emplace(path_key, units.size());
    units.push_back(BuildUnit {
        .source_path = normalized_path,
        .parse_result = std::move(*parse_result),
        .sema_result = std::move(sema_result),
        .mir_result = std::move(mir_result),
    });
    visiting.erase(path_key);
    return true;
}

std::unique_ptr<mc::mir::Module> MergeBuildUnits(const std::vector<BuildUnit>& units,
                                                 const mc::mir::Module& entry_module,
                                                 const std::filesystem::path& entry_source_path) {
    auto merged = std::make_unique<mc::mir::Module>();
    std::unordered_set<std::string> seen_imports;
    for (const auto& unit : units) {
        if (unit.source_path == std::filesystem::absolute(entry_source_path).lexically_normal()) {
            continue;
        }
        mc::mir::Module namespaced_module = *unit.mir_result.module;
        if (unit.source_path != std::filesystem::absolute(entry_source_path).lexically_normal()) {
            NamespaceImportedBuildUnit(namespaced_module, unit.source_path.stem().string());
        }
        for (const auto& import_name : unit.mir_result.module->imports) {
            if (seen_imports.insert(import_name).second) {
                merged->imports.push_back(import_name);
            }
        }
        merged->type_decls.insert(merged->type_decls.end(),
                                  namespaced_module.type_decls.begin(),
                                  namespaced_module.type_decls.end());
        merged->globals.insert(merged->globals.end(),
                               namespaced_module.globals.begin(),
                               namespaced_module.globals.end());
        merged->functions.insert(merged->functions.end(),
                                 namespaced_module.functions.begin(),
                                 namespaced_module.functions.end());
    }

    for (const auto& import_name : entry_module.imports) {
        if (seen_imports.insert(import_name).second) {
            merged->imports.push_back(import_name);
        }
    }
    merged->type_decls.insert(merged->type_decls.end(),
                              entry_module.type_decls.begin(),
                              entry_module.type_decls.end());
    merged->globals.insert(merged->globals.end(),
                           entry_module.globals.begin(),
                           entry_module.globals.end());
    merged->functions.insert(merged->functions.end(),
                             entry_module.functions.begin(),
                             entry_module.functions.end());
    return merged;
}

std::optional<CheckedProgram> CompileToMir(const CommandOptions& options,
                                           support::SourceManager& source_manager,
                                           support::DiagnosticSink& diagnostics,
                                           bool include_imports_for_build) {
    const auto file_id = source_manager.LoadFile(options.source_path, diagnostics);
    if (!file_id.has_value()) {
        return std::nullopt;
    }

    const support::SourceFile* source_file = source_manager.GetFile(*file_id);
    const auto lex_result = mc::lex::Lex(source_file->contents, source_file->path.generic_string(), diagnostics);
    auto parse_result = mc::parse::Parse(lex_result, source_file->path, diagnostics);
    if (!parse_result.ok) {
        return std::nullopt;
    }

    auto sema_result = mc::sema::CheckProgram(*parse_result.source_file,
                                              source_file->path,
                                              BuildSemaOptions(source_file->path, options),
                                              diagnostics);
    if (!sema_result.ok) {
        return std::nullopt;
    }
    if (!WriteModuleInterface(source_file->path,
                              MakeModuleInterfaceArtifact(source_file->path,
                                                          *parse_result.source_file,
                                                          *sema_result.module,
                                                          BootstrapTargetIdentity()),
                              options.build_dir,
                              diagnostics)) {
        return std::nullopt;
    }

    auto mir_result = mc::mir::LowerSourceFile(*parse_result.source_file, *sema_result.module, source_file->path, diagnostics);
    if (!mir_result.ok) {
        return std::nullopt;
    }

    if (!mc::mir::ValidateModule(*mir_result.module, source_file->path, diagnostics)) {
        return std::nullopt;
    }

    if (include_imports_for_build) {
        const std::filesystem::path normalized_entry_path = std::filesystem::absolute(source_file->path).lexically_normal();
        const std::string entry_key = normalized_entry_path.generic_string();
        std::unordered_set<std::string> visiting {entry_key};
        std::unordered_map<std::string, std::size_t> built_indices;
        std::vector<BuildUnit> units;

        const auto effective_import_roots = ComputeEffectiveImportRoots(normalized_entry_path, options.import_roots);
        for (const auto& import_decl : parse_result.source_file->imports) {
            const auto import_path = mc::support::ResolveImportPath(normalized_entry_path,
                                                                    import_decl.module_name,
                                                                    effective_import_roots,
                                                                    diagnostics,
                                                                    import_decl.span);
            if (!import_path.has_value()) {
                return std::nullopt;
            }
            if (!CollectBuildUnits(*import_path, options, diagnostics, visiting, built_indices, units)) {
                return std::nullopt;
            }
        }
        visiting.erase(entry_key);

        auto merged_module = MergeBuildUnits(units, *mir_result.module, normalized_entry_path);
        if (!mc::mir::ValidateModule(*merged_module, source_file->path, diagnostics)) {
            return std::nullopt;
        }
        mir_result.module = std::move(merged_module);
    }

    return CheckedProgram {
        .source_file = source_file,
        .parse_result = std::move(parse_result),
        .sema_result = std::move(sema_result),
        .mir_result = std::move(mir_result),
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
    return false;
}

struct ProjectBuildResult {
    std::vector<BuildUnit> units;
    std::filesystem::path executable_path;
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
    std::string output;
};

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

    support::DiagnosticSink diagnostics;
    if (!WriteTextArtifact(runner_path, BuildOrdinaryRunnerSource(ordinary_tests), "test runner source", diagnostics)) {
        std::cerr << diagnostics.Render() << '\n';
        return false;
    }

    CommandOptions runner_options;
    runner_options.source_path = runner_path;
    runner_options.build_dir = build_dir;
    runner_options.build_dir_explicit = true;
    runner_options.import_roots = BuildTestImportRoots(project, target, options.import_roots, test_roots, generated_root);

    if (RunBuild(runner_options) != 0) {
        return false;
    }

    const auto artifacts = support::ComputeBuildArtifactTargets(runner_path, build_dir);
    return RunExecutableCommand(artifacts.executable, {}) == 0;
}

CapturedCommandResult RunExecutableCapture(const std::filesystem::path& executable_path,
                                          const std::vector<std::string>& arguments,
                                          const std::filesystem::path& output_path) {
    std::filesystem::create_directories(output_path.parent_path());
    std::vector<std::string> command;
    command.reserve(arguments.size() + 1);
    command.push_back(executable_path.generic_string());
    command.insert(command.end(), arguments.begin(), arguments.end());
    const std::string shell = JoinCommand(command) + " >" + QuoteShellArg(output_path.generic_string()) + " 2>&1";
    const int raw_status = std::system(shell.c_str());
    CapturedCommandResult result;
    support::DiagnosticSink diagnostics;
    result.output = ReadSourceText(output_path, diagnostics).value_or("");
#ifdef WIFEXITED
    if (WIFEXITED(raw_status)) {
        result.exited = true;
        result.exit_code = WEXITSTATUS(raw_status);
    }
#endif
#ifdef WIFSIGNALED
    if (WIFSIGNALED(raw_status)) {
        result.signaled = true;
        result.signal_number = WTERMSIG(raw_status);
    }
#endif
    return result;
}

bool EvaluateCheckPassCase(const CompilerRegressionCase& regression_case,
                           const std::vector<std::filesystem::path>& import_roots,
                           const std::filesystem::path& build_dir,
                           std::string& failure_detail) {
    support::SourceManager source_manager;
    support::DiagnosticSink diagnostics;
    CommandOptions options;
    options.source_path = regression_case.source_path;
    options.build_dir = build_dir;
    options.build_dir_explicit = true;
    options.import_roots = import_roots;
    if (!CompileToMir(options, source_manager, diagnostics, false).has_value()) {
        failure_detail = diagnostics.Render();
        return false;
    }
    return true;
}

bool EvaluateCheckFailCase(const CompilerRegressionCase& regression_case,
                           const std::vector<std::filesystem::path>& import_roots,
                           const std::filesystem::path& build_dir,
                           std::string& failure_detail) {
    support::SourceManager source_manager;
    support::DiagnosticSink diagnostics;
    CommandOptions options;
    options.source_path = regression_case.source_path;
    options.build_dir = build_dir;
    options.build_dir_explicit = true;
    options.import_roots = import_roots;
    if (CompileToMir(options, source_manager, diagnostics, false).has_value()) {
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
    support::SourceManager source_manager;
    support::DiagnosticSink diagnostics;
    CommandOptions options;
    options.source_path = regression_case.source_path;
    options.build_dir = build_dir;
    options.build_dir_explicit = true;
    options.import_roots = import_roots;
    const auto checked = CompileToMir(options, source_manager, diagnostics, false);
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
                           std::string& failure_detail) {
    support::SourceManager source_manager;
    support::DiagnosticSink diagnostics;
    CommandOptions options;
    options.source_path = regression_case.source_path;
    options.build_dir = build_dir;
    options.build_dir_explicit = true;
    options.import_roots = import_roots;
    const auto checked = CompileToMir(options, source_manager, diagnostics, true);
    if (!checked.has_value()) {
        failure_detail = diagnostics.Render();
        return false;
    }

    const auto build_targets = support::ComputeBuildArtifactTargets(regression_case.source_path, build_dir);
    const auto build_result = mc::codegen_llvm::BuildExecutable(
        *checked->mir_result.module,
        regression_case.source_path,
        {
            .target = mc::codegen_llvm::BootstrapTargetConfig(),
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
                                                                  build_dir / (support::SanitizeArtifactStem(regression_case.source_path) + ".stdout.txt"));
    if (!run_result.exited || run_result.exit_code != regression_case.expected_exit_code || run_result.output != *expected) {
        failure_detail = "expected exit code " + std::to_string(regression_case.expected_exit_code) + " and stdout '" + *expected +
                         "', got exit=" + std::to_string(run_result.exit_code) + " stdout='" + run_result.output + "'";
        return false;
    }
    return true;
}

bool RunCompilerRegressionCases(const std::vector<CompilerRegressionCase>& regression_cases,
                                const std::vector<std::filesystem::path>& import_roots,
                                const std::filesystem::path& build_dir) {
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
                case_ok = EvaluateRunOutputCase(regression_case, import_roots, case_build_dir, failure_detail);
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
                    .message = "tests are not enabled for target '" + target->name + "'",
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

std::optional<ProjectBuildResult> BuildProjectTarget(TargetBuildGraph& graph,
                                                     support::DiagnosticSink& diagnostics) {
    auto units = CompileTargetUnits(graph, diagnostics, true);
    if (!units.has_value() || units->empty()) {
        return std::nullopt;
    }

    const BuildUnit& entry_unit = units->back();
    const auto build_targets = support::ComputeBuildArtifactTargets(entry_unit.source_path, graph.build_dir);
    const auto runtime_object_path = ComputeRuntimeObjectPath(entry_unit.source_path, graph.build_dir);
    const auto repo_root = DiscoverRepositoryRoot(entry_unit.source_path);
    const auto runtime_source_path = repo_root.has_value()
                                         ? std::optional<std::filesystem::path> {*repo_root / "runtime/hosted/mc_hosted_runtime.c"}
                                         : std::optional<std::filesystem::path> {};

    if (ShouldRelinkProjectExecutable(build_targets.executable, runtime_object_path, runtime_source_path, *units)) {
        std::vector<std::filesystem::path> object_paths;
        object_paths.reserve(units->size());
        for (const auto& unit : *units) {
            object_paths.push_back(unit.object_path);
        }

        const auto link_result = mc::codegen_llvm::LinkExecutable(entry_unit.source_path,
                                                                  {
                                                                      .target = graph.target_config,
                                                                      .object_paths = object_paths,
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
    };
}

int RunCheck(const CommandOptions& options) {
    support::DiagnosticSink diagnostics;
    if (ClassifyInvocation(options) == InvocationKind::kDirectSource) {
        support::SourceManager source_manager;
        const auto checked = CompileToMir(options, source_manager, diagnostics, false);
        if (!checked.has_value()) {
            std::cerr << diagnostics.Render() << '\n';
            return 1;
        }

        const auto targets = support::ComputeDumpTargets(checked->source_file->path, options.build_dir);

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
                                                                      checked->source_file->path,
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

        std::cout << "checked " << checked->source_file->path.generic_string()
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
    auto units = CompileTargetUnits(*graph, diagnostics, false);
    if (!units.has_value() || units->empty()) {
        std::cerr << diagnostics.Render() << '\n';
        return 1;
    }

    const BuildUnit& entry_unit = units->back();
    const auto dump_targets = support::ComputeDumpTargets(entry_unit.source_path, graph->build_dir);

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
                                      {.target = graph->target_config},
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
        support::SourceManager source_manager;
        const auto checked = CompileToMir(options, source_manager, diagnostics, true);
        if (!checked.has_value()) {
            std::cerr << diagnostics.Render() << '\n';
            return 1;
        }

        const auto dump_targets = support::ComputeDumpTargets(checked->source_file->path, options.build_dir);
        const auto build_targets = support::ComputeBuildArtifactTargets(checked->source_file->path, options.build_dir);

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
                                                                      checked->source_file->path,
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
            checked->source_file->path,
            {
                .target = mc::codegen_llvm::BootstrapTargetConfig(),
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

        std::cout << "built " << checked->source_file->path.generic_string() << " -> "
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

    const auto dump_targets = support::ComputeDumpTargets(entry_unit.source_path, graph->build_dir);
    const auto build_targets = support::ComputeBuildArtifactTargets(entry_unit.source_path, graph->build_dir);

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
                                      {.target = graph->target_config},
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

    std::cout << "built target " << graph->target.name << " -> "
              << build_result->executable_path.generic_string()
              << " (bootstrap phase 7 project graph)" << '\n';

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

    auto build_result = BuildProjectTarget(*graph, diagnostics);
    if (!build_result.has_value()) {
        std::cerr << diagnostics.Render() << '\n';
        return 1;
    }

    return RunExecutableCommand(build_result->executable_path, options.run_arguments);
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
                                                 graph->build_dir) && target_ok;
        }
        if (!discovered->regression_cases.empty()) {
            const auto import_roots = ComputeProjectImportRoots(*project, *target, options.import_roots);
            target_ok = RunCompilerRegressionCases(discovered->regression_cases,
                                                  import_roots,
                                                  graph->build_dir) && target_ok;
        }
        if (discovered->ordinary_tests.empty() && discovered->regression_cases.empty()) {
            std::cout << "no tests discovered for target " << target->name << '\n';
        }
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
        if (!options.has_value()) {
            PrintUsage(std::cerr);
            return 1;
        }

        return RunCheck(*options);
    }

    if (command == "build") {
        const auto options = ParseCommandOptions(argc, argv, 2, std::cerr);
        if (!options.has_value()) {
            PrintUsage(std::cerr);
            return 1;
        }

        return RunBuild(*options);
    }

    if (command == "run") {
        const auto options = ParseCommandOptions(argc, argv, 2, std::cerr);
        if (!options.has_value()) {
            PrintUsage(std::cerr);
            return 1;
        }

        return RunRun(*options);
    }

    if (command == "test") {
        const auto options = ParseCommandOptions(argc, argv, 2, std::cerr);
        if (!options.has_value()) {
            PrintUsage(std::cerr);
            return 1;
        }

        return RunTest(*options);
    }

    if (command == "dump-paths") {
        const auto options = ParseCommandOptions(argc, argv, 2, std::cerr);
        if (!options.has_value()) {
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
