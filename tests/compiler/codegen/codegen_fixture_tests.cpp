#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#include "compiler/codegen_llvm/backend.h"
#include "compiler/lex/lexer.h"
#include "compiler/mir/mir.h"
#include "compiler/parse/parser.h"
#include "compiler/sema/check.h"
#include "compiler/support/diagnostics.h"
#include "tests/support/fixture_utils.h"

namespace {

std::string LowerToBackendDump(const std::string& source_text,
                               const std::filesystem::path& source_path) {
    mc::support::DiagnosticSink diagnostics;
    const auto lexed = mc::lex::Lex(source_text, source_path.generic_string(), diagnostics);
    const auto parsed = mc::parse::Parse(lexed, source_path, diagnostics);
    if (!parsed.ok) {
        mc::test_support::Fail("fixture should parse successfully: " + source_path.generic_string() + "\n" + diagnostics.Render());
    }

    const auto checked = mc::sema::CheckProgram(*parsed.source_file, source_path, diagnostics);
    if (!checked.ok) {
        mc::test_support::Fail("fixture should pass semantic checking: " + source_path.generic_string() + "\n" + diagnostics.Render());
    }

    const auto lowered = mc::mir::LowerSourceFile(*parsed.source_file, *checked.module, source_path, diagnostics);
    if (!lowered.ok) {
        mc::test_support::Fail("fixture should lower to MIR successfully: " + source_path.generic_string() + "\n" + diagnostics.Render());
    }
    if (!mc::mir::ValidateModule(*lowered.module, source_path, diagnostics)) {
        mc::test_support::Fail("fixture MIR should validate successfully: " + source_path.generic_string() + "\n" + diagnostics.Render());
    }

    mc::codegen_llvm::LowerOptions options {
        .target = mc::codegen_llvm::BootstrapTargetConfig(),
    };
    const auto backend = mc::codegen_llvm::LowerModule(*lowered.module, source_path, options, diagnostics);
    if (!backend.ok) {
        mc::test_support::Fail("fixture should lower to backend successfully: " + source_path.generic_string() + "\n" + diagnostics.Render());
    }

    return mc::codegen_llvm::DumpModule(*backend.module);
}

void RunFixture(const mc::test_support::DiscoveredFixture& fixture) {
    const auto source_text = mc::test_support::ReadTextFile(fixture.source_path);
    const auto expected_text = mc::test_support::NormalizeFixtureText(mc::test_support::ReadTextFile(fixture.expectation_path));
    const auto actual_dump = mc::test_support::NormalizeFixtureText(LowerToBackendDump(source_text, fixture.source_path));
    const auto second_dump = mc::test_support::NormalizeFixtureText(LowerToBackendDump(source_text, fixture.source_path));

    if (actual_dump != second_dump) {
        mc::test_support::Fail("backend fixture should lower deterministically across repeated runs: " + fixture.source_path.generic_string());
    }

    if (actual_dump != expected_text) {
        std::cerr << "fixture mismatch for " << fixture.source_path.generic_string() << "\n";
        std::cerr << "expected:\n" << expected_text << "\n";
        std::cerr << "actual:\n" << actual_dump << "\n";
        std::exit(1);
    }
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        mc::test_support::Fail("expected source root argument");
    }

    const std::filesystem::path source_root = argv[1];
    const std::filesystem::path fixture_dir = source_root / "tests/compiler/codegen";
    const auto fixtures = mc::test_support::DiscoverAdjacentFixtures(fixture_dir, ".backend.txt");

    for (const auto& fixture : fixtures) {
        RunFixture(fixture);
    }

    return 0;
}
