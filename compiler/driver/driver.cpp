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

int RunCheck(const CommandOptions& options) {
    support::DiagnosticSink diagnostics;
    support::SourceManager source_manager;
    const auto file_id = source_manager.LoadFile(options.source_path, diagnostics);
    if (!file_id.has_value()) {
        std::cerr << diagnostics.Render() << '\n';
        return 1;
    }

    const support::SourceFile* source_file = source_manager.GetFile(*file_id);
    const auto lex_result = mc::lex::Lex(source_file->contents, source_file->path.generic_string(), diagnostics);
    const auto parse_result = mc::parse::Parse(lex_result, source_file->path, diagnostics);
    if (!parse_result.ok) {
        std::cerr << diagnostics.Render() << '\n';
        return 1;
    }

    mc::sema::CheckOptions sema_options;
    sema_options.import_roots.reserve(options.import_roots.size());
    for (const auto& import_root : options.import_roots) {
        sema_options.import_roots.push_back(std::filesystem::absolute(import_root).lexically_normal());
    }

    const auto sema_result = mc::sema::CheckProgram(*parse_result.source_file, source_file->path, sema_options, diagnostics);
    if (!sema_result.ok) {
        std::cerr << diagnostics.Render() << '\n';
        return 1;
    }

    const auto mir_result = mc::mir::LowerSourceFile(*parse_result.source_file, *sema_result.module, source_file->path, diagnostics);
    if (!mir_result.ok) {
        std::cerr << diagnostics.Render() << '\n';
        return 1;
    }

    if (!mc::mir::ValidateModule(*mir_result.module, source_file->path, diagnostics)) {
        std::cerr << diagnostics.Render() << '\n';
        return 1;
    }

    const auto targets = support::ComputeDumpTargets(source_file->path, options.build_dir);

    if (options.dump_ast) {
        std::filesystem::create_directories(targets.ast.parent_path());
        std::ofstream output(targets.ast, std::ios::binary);
        output << mc::ast::DumpSourceFile(*parse_result.source_file);
        if (!output) {
            diagnostics.Report({
                .file_path = targets.ast,
                .span = support::kDefaultSourceSpan,
                .severity = support::DiagnosticSeverity::kError,
                .message = "unable to write AST dump",
            });
            std::cerr << diagnostics.Render() << '\n';
            return 1;
        }
    }

    if (options.dump_mir) {
        std::filesystem::create_directories(targets.mir.parent_path());
        std::ofstream output(targets.mir, std::ios::binary);
        output << mc::mir::DumpModule(*mir_result.module);
        if (!output) {
            diagnostics.Report({
                .file_path = targets.mir,
                .span = support::kDefaultSourceSpan,
                .severity = support::DiagnosticSeverity::kError,
                .message = "unable to write MIR dump",
            });
            std::cerr << diagnostics.Render() << '\n';
            return 1;
        }
    }

    if (options.dump_backend) {
        mc::codegen_llvm::LowerOptions backend_options {
            .target = mc::codegen_llvm::BootstrapTargetConfig(),
        };
        const auto backend_result = mc::codegen_llvm::LowerModule(*mir_result.module,
                                                                  source_file->path,
                                                                  backend_options,
                                                                  diagnostics);
        if (!backend_result.ok) {
            std::cerr << diagnostics.Render() << '\n';
            return 1;
        }

        std::filesystem::create_directories(targets.backend.parent_path());
        std::ofstream output(targets.backend, std::ios::binary);
        output << mc::codegen_llvm::DumpModule(*backend_result.module);
        if (!output) {
            diagnostics.Report({
                .file_path = targets.backend,
                .span = support::kDefaultSourceSpan,
                .severity = support::DiagnosticSeverity::kError,
                .message = "unable to write backend dump",
            });
            std::cerr << diagnostics.Render() << '\n';
            return 1;
        }
    }

    std::cout << "checked " << source_file->path.generic_string() << " (bootstrap phase 3 sema, phase 4 MIR)" << '\n';

    if (options.emit_dump_paths) {
        PrintDumpTargets(targets, std::cout);
    }

    return 0;
}

int RunDumpPaths(const CommandOptions& options) {
    PrintDumpTargets(support::ComputeDumpTargets(options.source_path, options.build_dir), std::cout);
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
