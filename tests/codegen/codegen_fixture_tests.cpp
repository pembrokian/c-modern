#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "compiler/codegen_llvm/backend.h"
#include "compiler/lex/lexer.h"
#include "compiler/mir/mir.h"
#include "compiler/parse/parser.h"
#include "compiler/sema/check.h"
#include "compiler/support/diagnostics.h"

namespace {

struct FixtureCase {
    std::string source_name;
    std::string expected_output_name;
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

std::string NormalizeFixtureText(std::string text) {
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) {
        text.pop_back();
    }
    return text;
}

std::string LowerToBackendDump(const std::string& source_text,
                               const std::filesystem::path& source_path) {
    mc::support::DiagnosticSink diagnostics;
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
    if (!lowered.ok) {
        Fail("fixture should lower to MIR successfully: " + source_path.generic_string() + "\n" + diagnostics.Render());
    }
    if (!mc::mir::ValidateModule(*lowered.module, source_path, diagnostics)) {
        Fail("fixture MIR should validate successfully: " + source_path.generic_string() + "\n" + diagnostics.Render());
    }

    mc::codegen_llvm::LowerOptions options {
        .target = mc::codegen_llvm::BootstrapTargetConfig(),
    };
    const auto backend = mc::codegen_llvm::LowerModule(*lowered.module, source_path, options, diagnostics);
    if (!backend.ok) {
        Fail("fixture should lower to backend successfully: " + source_path.generic_string() + "\n" + diagnostics.Render());
    }

    return mc::codegen_llvm::DumpModule(*backend.module);
}

void RunFixture(const std::filesystem::path& fixture_dir,
                const FixtureCase& fixture) {
    const auto source_path = fixture_dir / fixture.source_name;
    const auto expected_path = fixture_dir / fixture.expected_output_name;

    const auto source_text = ReadTextFile(source_path);
    const auto expected_text = NormalizeFixtureText(ReadTextFile(expected_path));
    const auto actual_dump = NormalizeFixtureText(LowerToBackendDump(source_text, source_path));
    const auto second_dump = NormalizeFixtureText(LowerToBackendDump(source_text, source_path));

    if (actual_dump != second_dump) {
        Fail("backend fixture should lower deterministically across repeated runs: " + source_path.generic_string());
    }

    if (actual_dump != expected_text) {
        std::cerr << "fixture mismatch for " << source_path.generic_string() << "\n";
        std::cerr << "expected:\n" << expected_text << "\n";
        std::cerr << "actual:\n" << actual_dump << "\n";
        std::exit(1);
    }
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        Fail("expected source root argument");
    }

    const std::filesystem::path source_root = argv[1];
    const std::filesystem::path fixture_dir = source_root / "tests/codegen";
    const std::vector<FixtureCase> fixtures = {
        {"branch_direct_call.mc", "branch_direct_call.backend.txt"},
        {"checked_division.mc", "checked_division.backend.txt"},
        {"checked_shift.mc", "checked_shift.backend.txt"},
        {"direct_call_add.mc", "direct_call_add.backend.txt"},
        {"enum_payload_layout.mc", "enum_payload_layout.backend.txt"},
        {"explicit_conversion.mc", "explicit_conversion.backend.txt"},
        {"generic_box_i32.mc", "generic_box_i32.backend.txt"},
        {"low_level_intrinsics.mc", "low_level_intrinsics.backend.txt"},
        {"noalias_param.mc", "noalias_param.backend.txt"},
        {"pointer_int_conversion.mc", "pointer_int_conversion.backend.txt"},
        {"smoke_return_zero.mc", "smoke_return_zero.backend.txt"},
        {"branch_if_return.mc", "branch_if_return.backend.txt"},
        {"store_local_compare_branch.mc", "store_local_compare_branch.backend.txt"},
    };

    for (const auto& fixture : fixtures) {
        RunFixture(fixture_dir, fixture);
    }

    return 0;
}
