#include "compiler/driver/driver.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "compiler/ast/ast.h"
#include "compiler/codegen_llvm/backend.h"
#include "compiler/lex/lexer.h"
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
    "  mc build <source> [--build-dir <dir>] [--import-root <dir>] [--emit-dump-paths] [--dump-ast] [--dump-mir] [--dump-backend]\n"
    "  mc dump-paths <source> [--build-dir <dir>]\n"
    "  mc --help\n";

struct CommandOptions {
    std::filesystem::path source_path;
    std::filesystem::path build_dir = "build/debug";
    std::vector<std::filesystem::path> import_roots;
    bool emit_dump_paths = false;
    bool dump_ast = false;
    bool dump_mir = false;
    bool dump_backend = false;
};

void PrintUsage(std::ostream& stream) {
    stream << kUsage;
}

std::optional<CommandOptions> ParseCommandOptions(int argc,
                                                  const char* argv[],
                                                  int start_index,
                                                  std::ostream& errors) {
    if (start_index >= argc) {
        errors << "missing source path\n";
        return std::nullopt;
    }

    CommandOptions options;
    options.source_path = argv[start_index];

    for (int index = start_index + 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
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

std::optional<CheckedProgram> CompileToMir(const CommandOptions& options,
                                           support::SourceManager& source_manager,
                                           support::DiagnosticSink& diagnostics) {
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

    mc::sema::CheckOptions sema_options;
    sema_options.import_roots.reserve(options.import_roots.size());
    for (const auto& import_root : options.import_roots) {
        sema_options.import_roots.push_back(std::filesystem::absolute(import_root).lexically_normal());
    }

    auto sema_result = mc::sema::CheckProgram(*parse_result.source_file, source_file->path, sema_options, diagnostics);
    if (!sema_result.ok) {
        return std::nullopt;
    }

    auto mir_result = mc::mir::LowerSourceFile(*parse_result.source_file, *sema_result.module, source_file->path, diagnostics);
    if (!mir_result.ok) {
        return std::nullopt;
    }

    if (!mc::mir::ValidateModule(*mir_result.module, source_file->path, diagnostics)) {
        return std::nullopt;
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

int RunCheck(const CommandOptions& options) {
    support::DiagnosticSink diagnostics;
    support::SourceManager source_manager;
    const auto checked = CompileToMir(options, source_manager, diagnostics);
    if (!checked.has_value()) {
        std::cerr << diagnostics.Render() << '\n';
        return 1;
    }

    const auto targets = support::ComputeDumpTargets(checked->source_file->path, options.build_dir);

    if (options.dump_ast) {
        if (!WriteTextArtifact(targets.ast,
                               mc::ast::DumpSourceFile(*checked->parse_result.source_file),
                               "AST dump",
                               diagnostics)) {
            std::cerr << diagnostics.Render() << '\n';
            return 1;
        }
    }

    if (options.dump_mir) {
        if (!WriteTextArtifact(targets.mir,
                               mc::mir::DumpModule(*checked->mir_result.module),
                               "MIR dump",
                               diagnostics)) {
            std::cerr << diagnostics.Render() << '\n';
            return 1;
        }
    }

    if (options.dump_backend) {
        mc::codegen_llvm::LowerOptions backend_options {
            .target = mc::codegen_llvm::BootstrapTargetConfig(),
        };
        const auto backend_result = mc::codegen_llvm::LowerModule(*checked->mir_result.module,
                                                                  checked->source_file->path,
                                                                  backend_options,
                                                                  diagnostics);
        if (!backend_result.ok) {
            std::cerr << diagnostics.Render() << '\n';
            return 1;
        }

        if (!WriteTextArtifact(targets.backend,
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

int RunBuild(const CommandOptions& options) {
    support::DiagnosticSink diagnostics;
    support::SourceManager source_manager;
    const auto checked = CompileToMir(options, source_manager, diagnostics);
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
        if (!backend_result.ok) {
            std::cerr << diagnostics.Render() << '\n';
            return 1;
        }
        if (!WriteTextArtifact(dump_targets.backend,
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
