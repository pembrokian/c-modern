#include <filesystem>
#include <iostream>
#include <string>

#include "compiler/ast/ast.h"
#include "compiler/lex/lexer.h"
#include "compiler/parse/parser.h"
#include "compiler/support/diagnostics.h"
#include "tests/support/fixture_utils.h"

namespace {

void RunFixture(const mc::test_support::DiscoveredFixture& fixture) {
    mc::support::DiagnosticSink diagnostics;
    const auto source_text = mc::test_support::ReadTextFile(fixture.source_path);
    const auto expected_text = mc::test_support::ReadTextFile(fixture.expectation_path);

    const auto lexed = mc::lex::Lex(source_text, fixture.source_path.generic_string(), diagnostics);
    const auto parsed = mc::parse::Parse(lexed, fixture.source_path, diagnostics);

    if (fixture.expects_success) {
        if (!parsed.ok) {
            mc::test_support::Fail("fixture should parse successfully: " + fixture.source_path.generic_string() + "\n" + diagnostics.Render());
        }

        const auto actual_dump = mc::ast::DumpSourceFile(*parsed.source_file);
        if (actual_dump != expected_text) {
            std::cerr << "fixture mismatch for " << fixture.source_path.generic_string() << "\n";
            std::cerr << "expected:\n" << expected_text << "\n";
            std::cerr << "actual:\n" << actual_dump << "\n";
            std::exit(1);
        }
        return;
    }

    if (parsed.ok) {
        mc::test_support::Fail("fixture should fail to parse: " + fixture.source_path.generic_string());
    }

    mc::test_support::RequireExpectedDiagnosticSubstrings(diagnostics.Render(), expected_text, fixture.source_path);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        mc::test_support::Fail("expected source root argument");
    }

    const std::filesystem::path source_root = argv[1];
    const std::filesystem::path fixture_dir = source_root / "tests/parser";
    const auto fixtures = mc::test_support::DiscoverAdjacentFixtures(
        fixture_dir,
        ".ast.txt",
        ".errors.txt",
        mc::test_support::FixtureDiscoveryMode::kRequireExpectation);

    for (const auto& fixture : fixtures) {
        RunFixture(fixture);
    }

    return 0;
}