#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "compiler/lex/lexer.h"
#include "compiler/parse/parser.h"
#include "compiler/sema/check.h"
#include "compiler/support/diagnostics.h"

namespace {

struct FixtureCase {
    std::string source_name;
    std::string expected_output_name;
    std::vector<std::string> import_roots;
    bool should_check = true;
};

void Fail(const std::string& message) {
    std::cerr << "test failure: " << message << '\n';
    std::exit(1);
}

std::string ReadTextFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        Fail("unable to read fixture file: " + path.generic_string());
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

void RunFixture(const std::filesystem::path& source_root, const std::filesystem::path& fixture_dir, const FixtureCase& fixture) {
    const auto source_path = fixture_dir / fixture.source_name;
    const auto expected_path = fixture_dir / fixture.expected_output_name;

    mc::support::DiagnosticSink diagnostics;
    const auto source_text = ReadTextFile(source_path);
    const auto expected_text = ReadTextFile(expected_path);

    const auto lexed = mc::lex::Lex(source_text, source_path.generic_string(), diagnostics);
    const auto parsed = mc::parse::Parse(lexed, source_path, diagnostics);
    if (!parsed.ok) {
        Fail("fixture should parse successfully: " + source_path.generic_string() + "\n" + diagnostics.Render());
    }

    mc::sema::CheckOptions options;
    for (const auto& import_root : fixture.import_roots) {
        options.import_roots.push_back(source_root / import_root);
    }

    const auto checked = mc::sema::CheckProgram(*parsed.source_file, source_path, options, diagnostics);
    if (fixture.should_check) {
        if (!checked.ok) {
            Fail("fixture should pass semantic checking: " + source_path.generic_string() + "\n" + diagnostics.Render());
        }

        const auto actual_dump = mc::sema::DumpModule(*checked.module);
        if (actual_dump != expected_text) {
            std::cerr << "fixture mismatch for " << source_path.generic_string() << "\n";
            std::cerr << "expected:\n" << expected_text << "\n";
            std::cerr << "actual:\n" << actual_dump << "\n";
            std::exit(1);
        }
        return;
    }

    if (checked.ok) {
        Fail("fixture should fail semantic checking: " + source_path.generic_string());
    }

    const auto rendered = diagnostics.Render();
    std::istringstream expected_lines(expected_text);
    std::string expected_line;
    while (std::getline(expected_lines, expected_line)) {
        if (expected_line.empty()) {
            continue;
        }
        if (rendered.find(expected_line) == std::string::npos) {
            std::cerr << "missing expected diagnostic substring for " << source_path.generic_string() << "\n";
            std::cerr << "expected substring: " << expected_line << "\n";
            std::cerr << "actual diagnostics:\n" << rendered << "\n";
            std::exit(1);
        }
    }
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        Fail("expected source root argument");
    }

    const std::filesystem::path source_root = argv[1];
    const std::filesystem::path fixture_dir = source_root / "tests/sema";
    const std::vector<FixtureCase> fixtures = {
        {"basic_ok.mc", "basic_ok.sema.txt", {}, true},
        {"import_ok_main.mc", "import_ok_main.sema.txt", {}, true},
        {"import_type_ok_main.mc", "import_type_ok_main.sema.txt", {}, true},
        {"import_root_ok_main.mc", "import_root_ok_main.sema.txt", {"tests/sema/import_roots"}, true},
        {"layout_ok.mc", "layout_ok.sema.txt", {}, true},
        {"unknown_name_bad.mc", "unknown_name_bad.errors.txt", {}, false},
        {"unknown_callee_bad.mc", "unknown_callee_bad.errors.txt", {}, false},
        {"missing_import_bad.mc", "missing_import_bad.errors.txt", {}, false},
        {"import_cycle_a.mc", "import_cycle_a.errors.txt", {}, false},
        {"return_type_bad.mc", "return_type_bad.errors.txt", {}, false},
        {"break_outside_loop_bad.mc", "break_outside_loop_bad.errors.txt", {}, false},
        {"defer_non_call_bad.mc", "defer_non_call_bad.errors.txt", {}, false},
        {"call_arity_bad.mc", "call_arity_bad.errors.txt", {}, false},
        {"bare_conversion_syntax_bad.mc", "bare_conversion_syntax_bad.errors.txt", {}, false},
        {"layout_cycle_bad.mc", "layout_cycle_bad.errors.txt", {}, false},
    };

    for (const auto& fixture : fixtures) {
        RunFixture(source_root, fixture_dir, fixture);
    }

    return 0;
}