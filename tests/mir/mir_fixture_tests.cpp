#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
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
    std::string canonical_example_name;
    std::vector<std::string> import_roots;
    std::vector<std::pair<std::string, std::string>> imported_modules;
};

const std::vector<std::string> kForbiddenSemanticBoundaryCallTargets = {
    "volatile_load",
    "volatile_store",
    "atomic_load",
    "atomic_store",
    "atomic_exchange",
    "atomic_compare_exchange",
    "atomic_fetch_add",
    "mmio_ptr",
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

std::string LowerToValidatedDump(const std::string& source_text,
                                 const std::filesystem::path& source_path,
                                 const mc::sema::CheckOptions& options) {
    mc::support::DiagnosticSink diagnostics;
    const auto lexed = mc::lex::Lex(source_text, source_path.generic_string(), diagnostics);
    const auto parsed = mc::parse::Parse(lexed, source_path, diagnostics);
    if (!parsed.ok) {
        Fail("fixture should parse successfully: " + source_path.generic_string() + "\n" + diagnostics.Render());
    }

    const auto checked = mc::sema::CheckProgram(*parsed.source_file, source_path, options, diagnostics);
    if (!checked.ok) {
        Fail("fixture should pass semantic checking: " + source_path.generic_string() + "\n" + diagnostics.Render());
    }

    const auto lowered = mc::mir::LowerSourceFile(*parsed.source_file, *checked.module, source_path, diagnostics);
    if (!lowered.ok) {
        Fail("fixture should lower successfully: " + source_path.generic_string() + "\n" + diagnostics.Render());
    }
    if (!mc::mir::ValidateModule(*lowered.module, source_path, diagnostics)) {
        Fail("fixture should validate successfully: " + source_path.generic_string() + "\n" + diagnostics.Render());
    }

    return mc::mir::DumpModule(*lowered.module);
}

void VerifyNoSemanticBoundaryFallbackCalls(const std::string& dump,
                                          const std::filesystem::path& source_path) {
    for (const auto& target : kForbiddenSemanticBoundaryCallTargets) {
        const auto forbidden_call = "call target=" + target;
        if (dump.find(forbidden_call) != std::string::npos) {
            Fail("supported fixture must not use generic call shaping for semantic-boundary target " +
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
                Fail("supported fixture must preserve structured MIR " + std::string(description) + ": " +
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
    const auto example_text = ReadTextFile(example_path);
    if (example_text != source_text) {
        Fail("canonical example source must stay in sync with MIR fixture: " + example_path.generic_string());
    }
}

void RunFixture(const std::filesystem::path& source_root,
                const std::filesystem::path& fixture_dir,
                const FixtureCase& fixture) {
    const auto source_path = fixture_dir / fixture.source_name;
    const auto expected_path = fixture_dir / fixture.expected_output_name;

    mc::support::DiagnosticSink diagnostics;
    const auto source_text = ReadTextFile(source_path);
    const auto expected_text = NormalizeFixtureText(ReadTextFile(expected_path));

    VerifyCanonicalExampleSync(source_root, fixture, source_text);

    mc::sema::CheckOptions options;
    for (const auto& import_root : fixture.import_roots) {
        options.import_roots.push_back(source_root / import_root);
    }
    std::unordered_map<std::string, mc::sema::Module> imported_modules;
    for (const auto& imported_fixture : fixture.imported_modules) {
        const auto imported_path = fixture_dir / imported_fixture.second;
        const auto imported_text = ReadTextFile(imported_path);
        const auto imported_lexed = mc::lex::Lex(imported_text, imported_path.generic_string(), diagnostics);
        const auto imported_parsed = mc::parse::Parse(imported_lexed, imported_path, diagnostics);
        if (!imported_parsed.ok) {
            Fail("imported module fixture should parse successfully: " + imported_path.generic_string() + "\n" + diagnostics.Render());
        }
        const auto imported_checked = mc::sema::CheckProgram(*imported_parsed.source_file, imported_path, diagnostics);
        if (!imported_checked.ok) {
            Fail("imported module fixture should pass semantic checking: " + imported_path.generic_string() + "\n" + diagnostics.Render());
        }
        imported_modules[imported_fixture.first] = mc::sema::BuildImportVisibleModuleSurface(*imported_checked.module, *imported_parsed.source_file);
    }
    if (!imported_modules.empty()) {
        options.imported_modules = &imported_modules;
    }

    if (fixture.should_lower) {
        const auto actual_dump = NormalizeFixtureText(LowerToValidatedDump(source_text, source_path, options));
        const auto second_dump = NormalizeFixtureText(LowerToValidatedDump(source_text, source_path, options));
        if (actual_dump.find("unknown") != std::string::npos) {
            Fail("supported fixture should not emit unknown MIR types: " + source_path.generic_string() + "\n" + actual_dump);
        }
        if (actual_dump != second_dump) {
            Fail("supported fixture should lower deterministically across repeated runs: " + source_path.generic_string());
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
        Fail("fixture should parse successfully: " + source_path.generic_string() + "\n" + diagnostics.Render());
    }

    const auto checked = mc::sema::CheckProgram(*parsed.source_file, source_path, options, diagnostics);
    if (!checked.ok) {
        Fail("fixture should pass semantic checking: " + source_path.generic_string() + "\n" + diagnostics.Render());
    }

    const auto lowered = mc::mir::LowerSourceFile(*parsed.source_file, *checked.module, source_path, diagnostics);

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
        {"abi_c_struct.mc", "abi_c_struct.mir.txt", true, "", {}, {}},
        {"array_to_slice_conversion.mc", "array_to_slice_conversion.mir.txt", true, "", {}, {}},
        {"bounds_check.mc", "bounds_check.mir.txt", true, "", {}, {}},
        {"buffer_to_slice_conversion.mc", "buffer_to_slice_conversion.mir.txt", true, "", {}, {}},
        {"canonical_eval.mc", "canonical_eval.mir.txt", true, "eval.mc", {}, {}},
        {"canonical_memory_poll.mc", "canonical_memory_poll.mir.txt", true, "memory_poll.mc", {}, {}},
        {"canonical_window_stats.mc", "canonical_window_stats.mir.txt", true, "window_stats.mc", {}, {}},
        {"defer_immediate_args.mc", "defer_immediate_args.mir.txt", true, "", {}, {}},
        {"switch_expr.mc", "switch_expr.mir.txt", true, "", {}, {}},
        {"switch_case_defer.mc", "switch_case_defer.mir.txt", true, "", {}, {}},
        {"foreach_range_defer.mc", "foreach_range_defer.mir.txt", true, "", {}, {}},
        {"foreach_non_iterable_fail.mc", "foreach_non_iterable_fail.errors.txt", false, "", {}, {}},
        {"explicit_conversion.mc", "explicit_conversion.mir.txt", true, "", {}, {}},
        {"imported_generic_box_ok.mc", "imported_generic_box_ok.mir.txt", true, "", {}, {{"helper_box", "import_roots/helper_box.mc"}}},
        {"import_root_ok_main.mc", "import_root_ok_main.mir.txt", true, "", {"tests/mir/import_roots"}, {}},
        {"imported_atomic_ok.mc", "imported_atomic_ok.mir.txt", true, "", {}, {{"sync", "import_roots/sync_module.mc"}}},
        {"imported_event_buffer_ok.mc", "imported_event_buffer_ok.mir.txt", true, "", {"tests/mir/import_roots"}, {}},
        {"pointer_int_conversion.mc", "pointer_int_conversion.mir.txt", true, "", {}, {}},
        {"semantic_boundary_intrinsics.mc", "semantic_boundary_intrinsics.mir.txt", true, "", {}, {}},
        {"loop_iteration_defer.mc", "loop_iteration_defer.mir.txt", true, "", {}, {}},
        {"noalias_param_propagation.mc", "noalias_param_propagation.mir.txt", true, "", {}, {}},
        {"packed_struct.mc", "packed_struct.mir.txt", true, "", {}, {}},
        {"qualified_variant_import_root_ok.mc", "qualified_variant_import_root_ok.mir.txt", true, "", {}, {{"color_module", "import_roots/color_module.mc"}}},
        {"scoped_defer.mc", "scoped_defer.mir.txt", true, "", {}, {}},
        {"switch_variant.mc", "switch_variant.mir.txt", true, "", {}, {}},
        {"tuple_return_propagation.mc", "tuple_return_propagation.mir.txt", true, "", {}, {}},
        {"variant_binding_shadow_fail.mc", "variant_binding_shadow_fail.errors.txt", false, "", {}, {}},
    };

    for (const auto& fixture : fixtures) {
        RunFixture(source_root, fixture_dir, fixture);
    }

    return 0;
}