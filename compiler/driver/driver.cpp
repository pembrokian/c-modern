#include "compiler/driver/driver.h"

#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

#include "compiler/lex/lexer.h"
#include "compiler/parse/parser.h"
#include "compiler/support/diagnostics.h"
#include "compiler/support/dump_paths.h"
#include "compiler/support/source_manager.h"

namespace mc::driver {
namespace {

constexpr std::string_view kUsage =
    "Modern C bootstrap driver\n"
    "\n"
    "Usage:\n"
    "  mc check <source> [--build-dir <dir>] [--emit-dump-paths]\n"
    "  mc dump-paths <source> [--build-dir <dir>]\n"
    "  mc --help\n";

struct CommandOptions {
    std::filesystem::path source_path;
    std::filesystem::path build_dir = "build/debug";
    bool emit_dump_paths = false;
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

        if (argument == "--build-dir") {
            if (index + 1 >= argc) {
                errors << "missing value for --build-dir\n";
                return std::nullopt;
            }

            options.build_dir = argv[++index];
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
    const auto lex_result = mc::lex::LexForBootstrap(source_file->contents);
    const auto parse_result = mc::parse::ParseForBootstrap(lex_result);
    if (!parse_result.ok) {
        diagnostics.Report({
            .file_path = source_file->path,
            .span = support::kDefaultSourceSpan,
            .severity = support::DiagnosticSeverity::kError,
            .message = "bootstrap parser pipeline failed",
        });
        std::cerr << diagnostics.Render() << '\n';
        return 1;
    }

    std::cout << "checked " << source_file->path.generic_string() << " (bootstrap frontend stub)" << '\n';

    if (options.emit_dump_paths) {
        PrintDumpTargets(support::ComputeDumpTargets(source_file->path, options.build_dir), std::cout);
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
