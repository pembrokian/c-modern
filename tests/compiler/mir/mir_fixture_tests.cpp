#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "compiler/lex/lexer.h"
#include "compiler/mir/mir.h"
#include "compiler/parse/parser.h"
#include "compiler/sema/check.h"
#include "compiler/support/diagnostics.h"
#include "tests/support/fixture_utils.h"

namespace {

struct FixtureCase {
    std::string source_name;
    std::string expected_output_name;
    bool should_lower = true;
    std::string canonical_example_name;
    std::vector<std::string> import_roots;
    std::vector<std::pair<std::string, std::string>> imported_modules;
};

const std::vector<std::string> kForbiddenSemanticBoundaryCallTargets = {
    "memory_barrier",
    "volatile_load",
    "volatile_store",
    "atomic_load",
    "atomic_store",
    "atomic_exchange",
    "atomic_compare_exchange",
    "atomic_fetch_add",
    "mmio_ptr",
};

std::string LowerToValidatedDump(const std::string& source_text,
                                 const std::filesystem::path& source_path,
                                 const mc::sema::CheckOptions& options) {
    mc::support::DiagnosticSink diagnostics;
    const auto lexed = mc::lex::Lex(source_text, source_path.generic_string(), diagnostics);
    const auto parsed = mc::parse::Parse(lexed, source_path, diagnostics);
    if (!parsed.ok) {
        mc::test_support::Fail("fixture should parse successfully: " + source_path.generic_string() + "\n" + diagnostics.Render());
    }

    const auto checked = mc::sema::CheckProgram(*parsed.source_file, source_path, options, diagnostics);
    if (!checked.ok) {
        mc::test_support::Fail("fixture should pass semantic checking: " + source_path.generic_string() + "\n" + diagnostics.Render());
    }

    const auto lowered = mc::mir::LowerSourceFile(*parsed.source_file,
                                                  *checked.module,
                                                  source_path,
                                                  options.imported_modules,
                                                  diagnostics);
    if (!lowered.ok) {
        mc::test_support::Fail("fixture should lower successfully: " + source_path.generic_string() + "\n" + diagnostics.Render());
    }
    if (!mc::mir::ValidateModule(*lowered.module, source_path, diagnostics)) {
        mc::test_support::Fail("fixture should validate successfully: " + source_path.generic_string() + "\n" + diagnostics.Render());
    }

    return mc::mir::DumpModule(*lowered.module);
}

void VerifyNoSemanticBoundaryFallbackCalls(const std::string& dump,
                                          const std::filesystem::path& source_path) {
    for (const auto& target : kForbiddenSemanticBoundaryCallTargets) {
        const auto forbidden_call = "call target=" + target;
        if (dump.find(forbidden_call) != std::string::npos) {
            mc::test_support::Fail("supported fixture must not use generic call shaping for semantic-boundary target " +
                                   target + ": " + source_path.generic_string() + "\n" + dump);
        }
    }
}

void VerifyStructuredDumpMetadata(const std::string& dump,
                                  const std::filesystem::path& source_path) {
    std::istringstream lines(dump);
    std::string line;
    while (std::getline(lines, line)) {
        const auto require_token = [&](std::string_view token, std::string_view description) {
            if (line.find(token) == std::string::npos) {
                mc::test_support::Fail("supported fixture must preserve structured MIR " + std::string(description) + ": " +
                                       source_path.generic_string() + "\nline: " + line + "\nfull dump:\n" + dump);
            }
        };

        if (line.find(" target_kind=function") != std::string::npos || line.find(" target_kind=global") != std::string::npos ||
            line.find(" target_kind=field") != std::string::npos || line.find(" target_kind=deref_field") != std::string::npos) {
            require_token(" target_name=", "target name metadata");
        }
        if (line.rfind("      variant_match ", 0) == 0 || line.rfind("      variant_extract ", 0) == 0) {
            require_token(" target_name=", "variant target metadata");
            require_token(" target_base=", "variant base metadata");
        }
        if (line.rfind("      index ", 0) == 0) {
            require_token(" target_base=", "index base metadata");
            require_token(" target_types=", "index type metadata");
        }
        if (line.rfind("      slice ", 0) == 0) {
            require_token(" target_base=", "slice base metadata");
            require_token(" target_types=", "slice bound metadata");
        }
    }
}

void VerifyCanonicalExampleSync(const std::filesystem::path& source_root,
                                const FixtureCase& fixture,
                                const std::string& source_text) {
    if (fixture.canonical_example_name.empty()) {
        return;
    }

    const auto example_path = source_root / "examples/canonical" / fixture.canonical_example_name;
    const auto example_text = mc::test_support::ReadTextFile(example_path);
    if (example_text != source_text) {
        mc::test_support::Fail("canonical example source must stay in sync with MIR fixture: " + example_path.generic_string());
    }
}

FixtureCase BuildFixtureCase(const mc::test_support::DiscoveredFixture& fixture) {
    return {
        .source_name = fixture.source_name,
        .expected_output_name = fixture.expectation_name,
        .should_lower = fixture.expects_success,
    };
}

void RunFixture(const std::filesystem::path& source_root,
                const std::filesystem::path& fixture_dir,
                const FixtureCase& fixture) {
    const auto source_path = fixture_dir / fixture.source_name;
    const auto expected_path = fixture_dir / fixture.expected_output_name;

    mc::support::DiagnosticSink diagnostics;
    const auto source_text = mc::test_support::ReadTextFile(source_path);
    const auto expected_text = mc::test_support::NormalizeFixtureText(mc::test_support::ReadTextFile(expected_path));

    VerifyCanonicalExampleSync(source_root, fixture, source_text);

    mc::sema::CheckOptions options;
    for (const auto& import_root : fixture.import_roots) {
        options.import_roots.push_back(source_root / import_root);
    }
    std::unordered_map<std::string, mc::sema::Module> imported_modules;
    for (const auto& imported_fixture : fixture.imported_modules) {
        const auto imported_path = fixture_dir / imported_fixture.second;
        const auto imported_text = mc::test_support::ReadTextFile(imported_path);
        const auto imported_lexed = mc::lex::Lex(imported_text, imported_path.generic_string(), diagnostics);
        const auto imported_parsed = mc::parse::Parse(imported_lexed, imported_path, diagnostics);
        if (!imported_parsed.ok) {
            mc::test_support::Fail("imported module fixture should parse successfully: " + imported_path.generic_string() + "\n" + diagnostics.Render());
        }
        const auto imported_checked = mc::sema::CheckProgram(*imported_parsed.source_file, imported_path, diagnostics);
        if (!imported_checked.ok) {
            mc::test_support::Fail("imported module fixture should pass semantic checking: " + imported_path.generic_string() + "\n" + diagnostics.Render());
        }
        imported_modules[imported_fixture.first] = mc::sema::BuildImportVisibleModuleSurface(*imported_checked.module, *imported_parsed.source_file);
    }
    if (!imported_modules.empty()) {
        options.imported_modules = &imported_modules;
    }

    if (fixture.should_lower) {
        const auto actual_dump = mc::test_support::NormalizeFixtureText(LowerToValidatedDump(source_text, source_path, options));
        const auto second_dump = mc::test_support::NormalizeFixtureText(LowerToValidatedDump(source_text, source_path, options));
        if (actual_dump.find("unknown") != std::string::npos) {
            mc::test_support::Fail("supported fixture should not emit unknown MIR types: " + source_path.generic_string() + "\n" + actual_dump);
        }
        if (actual_dump != second_dump) {
            mc::test_support::Fail("supported fixture should lower deterministically across repeated runs: " + source_path.generic_string());
        }
        VerifyNoSemanticBoundaryFallbackCalls(actual_dump, source_path);
        VerifyStructuredDumpMetadata(actual_dump, source_path);
        if (actual_dump != expected_text) {
            std::cerr << "fixture mismatch for " << source_path.generic_string() << "\n";
            std::cerr << "expected:\n" << expected_text << "\n";
            std::cerr << "actual:\n" << actual_dump << "\n";
            std::exit(1);
        }
        return;
    }

    const auto lexed = mc::lex::Lex(source_text, source_path.generic_string(), diagnostics);
    const auto parsed = mc::parse::Parse(lexed, source_path, diagnostics);
    if (!parsed.ok) {
        mc::test_support::Fail("fixture should parse successfully: " + source_path.generic_string() + "\n" + diagnostics.Render());
    }

    const auto checked = mc::sema::CheckProgram(*parsed.source_file, source_path, options, diagnostics);
    if (!checked.ok) {
        mc::test_support::Fail("fixture should pass semantic checking: " + source_path.generic_string() + "\n" + diagnostics.Render());
    }

    const auto lowered = mc::mir::LowerSourceFile(*parsed.source_file,
                                                  *checked.module,
                                                  source_path,
                                                  options.imported_modules,
                                                  diagnostics);

    if (lowered.ok) {
        mc::test_support::Fail("fixture should fail to lower: " + source_path.generic_string());
    }

    mc::test_support::RequireExpectedDiagnosticSubstrings(diagnostics.Render(), expected_text, source_path);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        mc::test_support::Fail("expected source root argument");
    }

    const std::filesystem::path source_root = argv[1];
    const std::filesystem::path fixture_dir = source_root / "tests/compiler/mir";
    const std::vector<FixtureCase> explicit_fixtures = {
        {"canonical_eval.mc", "canonical_eval.mir.txt", true, "eval.mc", {}, {}},
        {"canonical_memory_poll.mc", "canonical_memory_poll.mir.txt", true, "memory_poll.mc", {}, {}},
        {"canonical_window_stats.mc", "canonical_window_stats.mir.txt", true, "window_stats.mc", {}, {}},
        {"canonical_array_grid_lookup.mc", "canonical_array_grid_lookup.mir.txt", true, "array_grid_lookup.mc", {}, {}},
        {"imported_generic_box_ok.mc", "imported_generic_box_ok.mir.txt", true, "", {}, {{"helper_box", "import_roots/helper_box.mc"}}},
        {"import_root_ok_main.mc", "import_root_ok_main.mir.txt", true, "", {"tests/compiler/mir/import_roots"}, {}},
        {"imported_atomic_ok.mc", "imported_atomic_ok.mir.txt", true, "", {}, {{"sync", "import_roots/sync_module.mc"}}},
        {"imported_event_buffer_ok.mc", "imported_event_buffer_ok.mir.txt", true, "", {"tests/compiler/mir/import_roots"}, {}},
        {"qualified_variant_import_root_ok.mc", "qualified_variant_import_root_ok.mir.txt", true, "", {}, {{"color_module", "import_roots/color_module.mc"}}},
        {"switch_imported_const_ok.mc", "switch_imported_const_ok.mir.txt", true, "", {}, {{"switch_consts", "import_roots/switch_consts.mc"}}},
        {"imported_array_extent_expr_ok.mc", "imported_array_extent_expr_ok.mir.txt", true, "", {}, {{"helper_extents", "import_roots/helper_extents.mc"}}},
        {"switch_shared_case_ok.mc", "switch_shared_case_ok.mir.txt", true, "", {}, {}},
    };

    std::unordered_set<std::string> explicit_sources;
    explicit_sources.reserve(explicit_fixtures.size());
    for (const auto& fixture : explicit_fixtures) {
        explicit_sources.insert(fixture.source_name);
        RunFixture(source_root, fixture_dir, fixture);
    }

    const auto discovered = mc::test_support::DiscoverAdjacentFixtures(
        fixture_dir,
        ".mir.txt",
        ".errors.txt",
        mc::test_support::FixtureDiscoveryMode::kExistingExpectationOnly);

    for (const auto& discovered_fixture : discovered) {
        if (explicit_sources.contains(discovered_fixture.source_name)) {
            continue;
        }
        RunFixture(source_root, fixture_dir, BuildFixtureCase(discovered_fixture));
    }

    return 0;
}