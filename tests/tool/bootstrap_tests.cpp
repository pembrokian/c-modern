#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "compiler/lex/lexer.h"
#include "compiler/parse/parser.h"
#include "compiler/support/diagnostics.h"
#include "compiler/support/dump_paths.h"
#include "compiler/support/source_manager.h"

namespace {

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "test failure: " << message << '\n';
        std::exit(1);
    }
}

std::filesystem::path MakeTempFile() {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto path = std::filesystem::temp_directory_path() /
                      ("c_modern_phase1_" + std::to_string(stamp) + ".mc");
    std::ofstream output(path);
    output << "func main() {}\n";
    return path;
}

void TestDiagnosticFormatting() {
    const mc::support::Diagnostic diagnostic {
        .file_path = "tests/cases/hello.mc",
        .span = {{3, 7}, {3, 12}},
        .severity = mc::support::DiagnosticSeverity::kError,
        .message = "expected declaration",
    };

    Expect(mc::support::FormatDiagnostic(diagnostic) ==
               "tests/cases/hello.mc:3:7: error: expected declaration",
           "diagnostic rendering should be stable");
}

void TestSourceManagerLoad() {
    const auto temp_file = MakeTempFile();
    mc::support::DiagnosticSink diagnostics;
    mc::support::SourceManager source_manager;

    const auto file_id = source_manager.LoadFile(temp_file, diagnostics);
    Expect(file_id.has_value(), "source manager should load existing files");
    Expect(!diagnostics.HasErrors(), "loading an existing file should not report errors");

    const auto* file = source_manager.GetFile(*file_id);
    Expect(file != nullptr, "loaded file should be retrievable");
    Expect(file->contents.find("func main") != std::string::npos, "loaded contents should match the file");

    std::filesystem::remove(temp_file);
}

void TestDumpPaths() {
    const auto targets = mc::support::ComputeDumpTargets("tests/cases/hello.mc", "build/debug");
    Expect(targets.ast.generic_string() == "build/debug/dumps/ast/tests_cases_hello.mc.ast.txt",
           "ast dump path should be deterministic");
    Expect(targets.mir.generic_string() == "build/debug/dumps/mir/tests_cases_hello.mc.mir.txt",
           "mir dump path should be deterministic");
    Expect(targets.mci.generic_string() == "build/debug/mci/tests_cases_hello.mc.mci",
           "mci path should be deterministic");
}

void TestBootstrapFrontendContracts() {
    const auto lexed = mc::lex::LexForBootstrap("func main() {}\n");
    const auto parsed = mc::parse::ParseForBootstrap(lexed);
    Expect(lexed.complete, "bootstrap lexer should report completion");
    Expect(!lexed.tokens.empty(), "bootstrap lexer should produce at least eof");
    Expect(parsed.ok, "bootstrap parser should accept bootstrap lexer output");
    Expect(parsed.stage == "phase1-bootstrap", "bootstrap parser should expose a stable stage label");
}

}  // namespace

int main() {
    TestDiagnosticFormatting();
    TestSourceManagerLoad();
    TestDumpPaths();
    TestBootstrapFrontendContracts();
    return 0;
}
