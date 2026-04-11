#include "compiler/driver/driver.h"
#include "compiler/driver/internal.h"

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

InvocationKind ClassifyInvocation(const CommandOptions& options) {
    return options.project_path.has_value() || options.target_name.has_value() || options.source_path.empty()
               ? InvocationKind::kProjectTarget
               : InvocationKind::kDirectSource;
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
    const std::string total_tests = std::to_string(ordinary_tests.size());
    source << "import errors\n";
    source << "import fmt\n";
    source << "import io\n";
    source << "import mem\n";
    for (const auto& ordinary_test : ordinary_tests) {
        source << "import " << ordinary_test.module_name << "\n";
    }
    source << "\n";
    source << "func write_summary(total: i32, failures: i32) i32 {\n";
    source << "    alloc: *mem.Allocator = mem.default_allocator()\n";
    source << "    passed: i32 = total - failures\n";
    source << "    total_buf: *Buffer<u8>\n";
    source << "    passed_buf: *Buffer<u8>\n";
    source << "    failed_buf: *Buffer<u8>\n";
    source << "    total_err: errors.Error\n";
    source << "    passed_err: errors.Error\n";
    source << "    failed_err: errors.Error\n";
    source << "    total_buf, total_err = fmt.sprint_i32(alloc, total)\n";
    source << "    passed_buf, passed_err = fmt.sprint_i32(alloc, passed)\n";
    source << "    failed_buf, failed_err = fmt.sprint_i32(alloc, failures)\n";
    source << "    if total_err != 0 || passed_err != 0 || failed_err != 0 {\n";
    source << "        if total_buf != nil {\n";
    source << "            mem.buffer_free<u8>(total_buf)\n";
    source << "        }\n";
    source << "        if passed_buf != nil {\n";
    source << "            mem.buffer_free<u8>(passed_buf)\n";
    source << "        }\n";
    source << "        if failed_buf != nil {\n";
    source << "            mem.buffer_free<u8>(failed_buf)\n";
    source << "        }\n";
    source << "        return 1\n";
    source << "    }\n";
    source << "    if total_buf == nil || passed_buf == nil || failed_buf == nil {\n";
    source << "        if total_buf != nil {\n";
    source << "            mem.buffer_free<u8>(total_buf)\n";
    source << "        }\n";
    source << "        if passed_buf != nil {\n";
    source << "            mem.buffer_free<u8>(passed_buf)\n";
    source << "        }\n";
    source << "        if failed_buf != nil {\n";
    source << "            mem.buffer_free<u8>(failed_buf)\n";
    source << "        }\n";
    source << "        return 1\n";
    source << "    }\n";
    source << "    defer mem.buffer_free<u8>(total_buf)\n";
    source << "    defer mem.buffer_free<u8>(passed_buf)\n";
    source << "    defer mem.buffer_free<u8>(failed_buf)\n";
    source << "    total_bytes: Slice<u8> = mem.slice_from_buffer<u8>(total_buf)\n";
    source << "    passed_bytes: Slice<u8> = mem.slice_from_buffer<u8>(passed_buf)\n";
    source << "    failed_bytes: Slice<u8> = mem.slice_from_buffer<u8>(failed_buf)\n";
    source << "    total_text: str = str{ ptr: total_bytes.ptr, len: total_bytes.len }\n";
    source << "    passed_text: str = str{ ptr: passed_bytes.ptr, len: passed_bytes.len }\n";
    source << "    failed_text: str = str{ ptr: failed_bytes.ptr, len: failed_bytes.len }\n";
    source << "    if io.write(total_text) != 0 {\n";
    source << "        return 1\n";
    source << "    }\n";
    source << "    if io.write(\" tests, \" ) != 0 {\n";
    source << "        return 1\n";
    source << "    }\n";
    source << "    if io.write(passed_text) != 0 {\n";
    source << "        return 1\n";
    source << "    }\n";
    source << "    if io.write(\" passed, \" ) != 0 {\n";
    source << "        return 1\n";
    source << "    }\n";
    source << "    if io.write(failed_text) != 0 {\n";
    source << "        return 1\n";
    source << "    }\n";
    source << "    if io.write_line(\" failed\") != 0 {\n";
    source << "        return 1\n";
    source << "    }\n";
    source << "    return 0\n";
    source << "}\n";
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
    source << "    if write_summary(" << total_tests << ", failures) != 0 {\n";
    source << "        return 1\n";
    source << "    }\n";
    source << "    if failures == 0 {\n";
    source << "        return 0\n";
    source << "    }\n";
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
    runner_target.package_name = target.package_name;
    runner_target.root = runner_path;
    runner_target.mode = test_mode;
    runner_target.env = options.env_override.value_or(target.env);
    runner_target.target = target.target;
    runner_target.links = target.links;
    runner_target.link_inputs = target.link_inputs;
    runner_target.module_search_paths = BuildTestImportRoots(project, target, options.import_roots, test_roots, generated_root);
    runner_target.package_roots = target.package_roots;
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
    const auto dump_targets = support::ComputeLogicalDumpTargets(entry_unit.artifact_key, graph->compile_graph.build_dir);

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

    const auto dump_targets = support::ComputeLogicalDumpTargets(entry_unit.artifact_key, graph->compile_graph.build_dir);
    const auto build_targets = support::ComputeLogicalBuildArtifactTargets(entry_unit.artifact_key, graph->compile_graph.build_dir);

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
