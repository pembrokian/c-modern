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

}  // namespace

std::optional<DirectSourceBuildResult> BuildDirectSource(const CommandOptions& options,
                                                         support::DiagnosticSink& diagnostics) {
    const auto checked = CompileToMir(options, diagnostics, true);
    if (!checked.has_value()) {
        return std::nullopt;
    }

    const auto dump_targets = support::ComputeDumpTargets(checked->source_path, options.build_dir);
    const auto build_targets = support::ComputeBuildArtifactTargets(checked->source_path, options.build_dir);

    if (options.dump_ast &&
        !WriteTextArtifact(dump_targets.ast,
                           mc::ast::DumpSourceFile(*checked->parse_result.source_file),
                           "AST dump",
                           diagnostics)) {
        return std::nullopt;
    }

    if (options.dump_mir &&
        !WriteTextArtifact(dump_targets.mir,
                           mc::mir::DumpModule(*checked->mir_result.module),
                           "MIR dump",
                           diagnostics)) {
        return std::nullopt;
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
            return std::nullopt;
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
        return std::nullopt;
    }

    return DirectSourceBuildResult {
        .source_path = checked->source_path,
        .dump_targets = dump_targets,
        .build_targets = build_targets,
        .executable_path = build_result.artifacts.executable_path,
    };
}

void PrintDirectSourceBuildSummary(const DirectSourceBuildResult& build_result,
                                   bool emit_dump_paths) {
    std::cout << "built " << build_result.source_path.generic_string() << " -> "
              << build_result.executable_path.generic_string()
              << " (bootstrap phase 5 executable path)" << '\n';

    if (emit_dump_paths) {
        PrintArtifactTargets(build_result.dump_targets, build_result.build_targets, std::cout);
    }
}

std::string MergeRenderedDiagnostics(std::string_view primary,
                                     std::string_view secondary) {
    if (primary.empty()) {
        return std::string(secondary);
    }
    if (secondary.empty()) {
        return std::string(primary);
    }

    std::string merged(primary);
    if (!merged.empty() && merged.back() != '\n') {
        merged.push_back('\n');
    }
    merged.append(secondary);
    return merged;
}


namespace {

int RunBuild(const CommandOptions& options);

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

namespace {

bool CollectAllGraphSourcePaths(const TargetBuildGraph& graph,
                                std::filesystem::file_time_type cutoff) {
    for (const auto& node : graph.compile_graph.nodes) {
        for (const auto& source_path : node.source_paths) {
            std::error_code ec;
            const auto mtime = std::filesystem::last_write_time(source_path, ec);
            if (ec || mtime >= cutoff) {
                return false;
            }
        }
    }
    for (const auto& linked : graph.linked_targets) {
        if (!CollectAllGraphSourcePaths(linked, cutoff)) {
            return false;
        }
    }
    return true;
}

bool IsProjectBuildFreshForDump(const TargetBuildGraph& graph) {
    if (graph.compile_graph.nodes.empty()) {
        return false;
    }
    const auto& entry_node = graph.compile_graph.nodes.back();
    const auto dump_targets = support::ComputeLogicalDumpTargets(entry_node.artifact_key, graph.compile_graph.build_dir);
    const auto build_targets = support::ComputeLogicalBuildArtifactTargets(entry_node.artifact_key, graph.compile_graph.build_dir);
    const bool is_exe = IsExecutableTargetKind(graph.target.kind);
    const std::filesystem::path& primary_artifact = is_exe ? build_targets.executable : build_targets.static_library;
    std::error_code ec;
    if (!std::filesystem::exists(dump_targets.mir, ec) || ec) {
        return false;
    }
    if (!std::filesystem::exists(primary_artifact, ec) || ec) {
        return false;
    }
    const auto dump_time = std::filesystem::last_write_time(dump_targets.mir, ec);
    if (ec) {
        return false;
    }
    const auto build_time = std::filesystem::last_write_time(primary_artifact, ec);
    if (ec) {
        return false;
    }
    const auto cutoff = std::min(dump_time, build_time);
    return CollectAllGraphSourcePaths(graph, cutoff);
}

}  // namespace

int RunBuild(const CommandOptions& options) {
    support::DiagnosticSink diagnostics;
    if (ClassifyInvocation(options) == InvocationKind::kDirectSource) {
        const auto build_result = BuildDirectSource(options, diagnostics);
        if (!build_result.has_value()) {
            std::cerr << diagnostics.Render() << '\n';
            return 1;
        }

        PrintDirectSourceBuildSummary(*build_result, options.emit_dump_paths);

        return 0;
    }

    auto graph = BuildTargetGraph(options, diagnostics);
    if (!graph.has_value()) {
        std::cerr << diagnostics.Render() << '\n';
        return 1;
    }

    if (options.dump_mir && !options.dump_ast && !options.dump_backend && IsProjectBuildFreshForDump(*graph)) {
        std::cout << "target " << graph->target.name << " up to date\n";
        return 0;
    }

    auto build_result = BuildProjectTarget(*graph, diagnostics);
    if (!build_result.has_value() || build_result->units.empty()) {
        std::cerr << diagnostics.Render() << '\n';
        return 1;
    }

    const BuildUnit& entry_unit = build_result->units.back();
    auto merged_module = MergeBuildUnits(build_result->units, *entry_unit.mir_result.module, entry_unit.source_path, diagnostics);
    if (!merged_module) {
        std::cerr << diagnostics.Render() << '\n';
        return 1;
    }
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
        support::DiagnosticSink diagnostics;
        const auto build_result = BuildDirectSource(options, diagnostics);
        if (!build_result.has_value()) {
            std::cerr << diagnostics.Render() << '\n';
            return 1;
        }

        PrintDirectSourceBuildSummary(*build_result, options.emit_dump_paths);
        return RunExecutableCommand(build_result->executable_path, options.run_arguments);
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

        return RunTestCommand(*options);
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
