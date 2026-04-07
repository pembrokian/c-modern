#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "compiler/ast/ast.h"
#include "compiler/lex/lexer.h"
#include "compiler/parse/parser.h"
#include "compiler/support/diagnostics.h"

namespace {

struct FixtureCase {
    std::string source_name;
    std::string expected_output_name;
    bool should_parse = true;
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

    if (fixture.should_parse) {
        if (!parsed.ok) {
            Fail("fixture should parse successfully: " + source_path.generic_string() + "\n" + diagnostics.Render());
        }

        const auto actual_dump = mc::ast::DumpSourceFile(*parsed.source_file);
        if (actual_dump != expected_text) {
            std::cerr << "fixture mismatch for " << source_path.generic_string() << "\n";
            std::cerr << "expected:\n" << expected_text << "\n";
            std::cerr << "actual:\n" << actual_dump << "\n";
            std::exit(1);
        }
        return;
    }

    if (parsed.ok) {
        Fail("fixture should fail to parse: " + source_path.generic_string());
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
    const std::filesystem::path fixture_dir = source_root / "tests/parser";
    const std::vector<FixtureCase> fixtures = {
        {"aggregate_named_fields.mc", "aggregate_named_fields.ast.txt", true},
        {"binding_or_assign_ambiguous.mc", "binding_or_assign_ambiguous.ast.txt", true},
        {"generic_variant_ctor_ok.mc", "generic_variant_ctor_ok.ast.txt", true},
        {"internal.mc", "internal.ast.txt", true},
        {"private_decl_kinds.mc", "private_decl_kinds.ast.txt", true},
        {"qualified_variant_case.mc", "qualified_variant_case.ast.txt", true},
        {"switch_empty_default.mc", "switch_empty_default.ast.txt", true},
        {"export_block_cutover_bad.mc", "export_block_cutover_bad.errors.txt", false},
        {"missing_param_paren.mc", "missing_param_paren.errors.txt", false},
        {"missing_block_brace.mc", "missing_block_brace.errors.txt", false},
        {"switch_case_bad.mc", "switch_case_bad.errors.txt", false},
        {"deep_type_nesting_bad.mc", "deep_type_nesting_bad.errors.txt", false},
        {"deep_unary_nesting_bad.mc", "deep_unary_nesting_bad.errors.txt", false},
        {"type_args_bad.mc", "type_args_bad.errors.txt", false},
        {"aggregate_bad.mc", "aggregate_bad.errors.txt", false},
        {"unterminated_string_bad.mc", "unterminated_string_bad.errors.txt", false},
        {"bad_escape_bad.mc", "bad_escape_bad.errors.txt", false},
        {"bad_hex_bad.mc", "bad_hex_bad.errors.txt", false},
        {"trailing_separator_bad.mc", "trailing_separator_bad.errors.txt", false},
        {"double_separator_bad.mc", "double_separator_bad.errors.txt", false},
        {"bad_exponent_bad.mc", "bad_exponent_bad.errors.txt", false},
        {"exponent_separator_bad.mc", "exponent_separator_bad.errors.txt", false},
        {"missing_fraction_digit_bad.mc", "missing_fraction_digit_bad.errors.txt", false},
    };

    for (const auto& fixture : fixtures) {
        RunFixture(fixture_dir, fixture);
    }

    return 0;
}