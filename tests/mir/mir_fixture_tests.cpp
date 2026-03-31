#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "compiler/lex/lexer.h"
#include "compiler/mir/mir.h"
#include "compiler/parse/parser.h"
#include "compiler/sema/check.h"
#include "compiler/support/diagnostics.h"

namespace {

struct FixtureCase {
    std::string source_name;
    std::string expected_output_name;
    bool should_lower = true;
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

void RunFixture(const std::filesystem::path& fixture_dir, const FixtureCase& fixture) {
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

    const auto checked = mc::sema::CheckProgram(*parsed.source_file, source_path, diagnostics);
    if (!checked.ok) {
        Fail("fixture should pass semantic checking: " + source_path.generic_string() + "\n" + diagnostics.Render());
    }

    const auto lowered = mc::mir::LowerSourceFile(*parsed.source_file, *checked.module, source_path, diagnostics);
    if (fixture.should_lower) {
        if (!lowered.ok) {
            Fail("fixture should lower successfully: " + source_path.generic_string() + "\n" + diagnostics.Render());
        }
        if (!mc::mir::ValidateModule(*lowered.module, source_path, diagnostics)) {
            Fail("fixture should validate successfully: " + source_path.generic_string() + "\n" + diagnostics.Render());
        }

        const auto actual_dump = mc::mir::DumpModule(*lowered.module);
        if (actual_dump != expected_text) {
            std::cerr << "fixture mismatch for " << source_path.generic_string() << "\n";
            std::cerr << "expected:\n" << expected_text << "\n";
            std::cerr << "actual:\n" << actual_dump << "\n";
            std::exit(1);
        }
        return;
    }

    if (lowered.ok) {
        Fail("fixture should fail to lower: " + source_path.generic_string());
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
    const std::filesystem::path fixture_dir = source_root / "tests/mir";
    const std::vector<FixtureCase> fixtures = {
        {"array_to_slice_conversion.mc", "array_to_slice_conversion.mir.txt", true},
        {"bounds_check.mc", "bounds_check.mir.txt", true},
        {"buffer_to_slice_conversion.mc", "buffer_to_slice_conversion.mir.txt", true},
        {"defer_immediate_args.mc", "defer_immediate_args.mir.txt", true},
        {"switch_expr.mc", "switch_expr.mir.txt", true},
        {"switch_case_defer.mc", "switch_case_defer.mir.txt", true},
        {"foreach_range_defer.mc", "foreach_range_defer.mir.txt", true},
        {"explicit_conversion.mc", "explicit_conversion.mir.txt", true},
        {"pointer_int_conversion.mc", "pointer_int_conversion.mir.txt", true},
        {"semantic_boundary_intrinsics.mc", "semantic_boundary_intrinsics.mir.txt", true},
        {"loop_iteration_defer.mc", "loop_iteration_defer.mir.txt", true},
        {"scoped_defer.mc", "scoped_defer.mir.txt", true},
        {"switch_variant.mc", "switch_variant.mir.txt", true},
    };

    for (const auto& fixture : fixtures) {
        RunFixture(fixture_dir, fixture);
    }

    return 0;
}