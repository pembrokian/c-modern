#include <filesystem>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "compiler/lex/lexer.h"
#include "compiler/parse/parser.h"
#include "compiler/sema/check.h"
#include "compiler/support/diagnostics.h"
#include "tests/support/fixture_utils.h"

namespace {

struct FixtureCase {
    std::string source_name;
    std::string expected_output_name;
    std::vector<std::string> import_roots;
    bool should_check = true;
    std::optional<std::string> current_package_identity;
    std::vector<std::pair<std::string, std::string>> package_roots;
};

bool PathWithinRoot(const std::filesystem::path& path, const std::filesystem::path& root) {
    auto path_it = path.begin();
    auto root_it = root.begin();
    while (root_it != root.end()) {
        if (path_it == path.end() || *path_it != *root_it) {
            return false;
        }
        ++path_it;
        ++root_it;
    }
    return true;
}

mc::parse::ParseResult ParseText(const std::string& source,
                                 const std::filesystem::path& path,
                                 mc::support::DiagnosticSink& diagnostics) {
    const auto lexed = mc::lex::Lex(source, path.generic_string(), diagnostics);
    return mc::parse::Parse(lexed, path, diagnostics);
}

mc::sema::CheckResult CheckText(const std::string& source,
                                const std::filesystem::path& path,
                                mc::support::DiagnosticSink& diagnostics,
                                const mc::sema::CheckOptions& options = {}) {
    const auto parsed = ParseText(source, path, diagnostics);
    if (!parsed.ok) {
        mc::test_support::Fail("source should parse successfully: " + path.generic_string() + "\n" + diagnostics.Render());
    }
    return mc::sema::CheckProgram(*parsed.source_file, path, options, diagnostics);
}

FixtureCase BuildFixtureCase(const mc::test_support::DiscoveredFixture& fixture) {
    return {
        .source_name = fixture.source_name,
        .expected_output_name = fixture.expectation_name,
        .import_roots = {},
        .should_check = fixture.expects_success,
    };
}

void RunFixture(const std::filesystem::path& source_root, const std::filesystem::path& fixture_dir, const FixtureCase& fixture) {
    const auto source_path = fixture_dir / fixture.source_name;
    const auto expected_path = fixture_dir / fixture.expected_output_name;

    mc::support::DiagnosticSink diagnostics;
    const auto source_text = mc::test_support::ReadTextFile(source_path);
    const auto expected_text = mc::test_support::NormalizeFixtureText(mc::test_support::ReadTextFile(expected_path));

    const auto lexed = mc::lex::Lex(source_text, source_path.generic_string(), diagnostics);
    const auto parsed = mc::parse::Parse(lexed, source_path, diagnostics);
    if (!parsed.ok) {
        mc::test_support::Fail("fixture should parse successfully: " + source_path.generic_string() + "\n" + diagnostics.Render());
    }

    mc::sema::CheckOptions options;
    for (const auto& import_root : fixture.import_roots) {
        options.import_roots.push_back(source_root / import_root);
    }
    options.current_package_identity = fixture.current_package_identity;
    if (!fixture.package_roots.empty()) {
        std::vector<std::pair<std::filesystem::path, std::string>> package_roots;
        package_roots.reserve(fixture.package_roots.size());
        for (const auto& [relative_root, package_identity] : fixture.package_roots) {
            package_roots.push_back({
                std::filesystem::absolute(fixture_dir / relative_root).lexically_normal(),
                package_identity,
            });
        }
        options.package_identity_for_source =
            [package_roots = std::move(package_roots)](const std::filesystem::path& source_path) -> std::optional<std::string> {
            const auto normalized_path = std::filesystem::absolute(source_path).lexically_normal();
            for (const auto& [root, package_identity] : package_roots) {
                if (PathWithinRoot(normalized_path, root)) {
                    return package_identity;
                }
            }
            return std::nullopt;
        };
    }

    const auto checked = mc::sema::CheckProgram(*parsed.source_file, source_path, options, diagnostics);
    if (fixture.should_check) {
        if (!checked.ok) {
            mc::test_support::Fail("fixture should pass semantic checking: " + source_path.generic_string() + "\n" + diagnostics.Render());
        }

        const auto actual_dump = mc::test_support::NormalizeFixtureText(mc::sema::DumpModule(*checked.module));
        if (actual_dump != expected_text) {
            std::cerr << "fixture mismatch for " << source_path.generic_string() << "\n";
            std::cerr << "expected:\n" << expected_text << "\n";
            std::cerr << "actual:\n" << actual_dump << "\n";
            std::exit(1);
        }
        return;
    }

    if (checked.ok) {
        mc::test_support::Fail("fixture should fail semantic checking: " + source_path.generic_string());
    }

    mc::test_support::RequireExpectedDiagnosticSubstrings(diagnostics.Render(), expected_text, source_path);
}

void TestImportedModuleSurfaceQualifiesNamedTypes() {
    mc::support::DiagnosticSink diagnostics;
    const auto parsed = ParseText(
        "import mem\n"
        "\n"
        "func expect_alloc(alloc: *mem.Allocator) i32 {\n"
        "    return 0\n"
        "}\n"
        "\n"
        "func main() i32 {\n"
        "    return expect_alloc(mem.default_allocator())\n"
        "}\n",
        "<imported-module-surface>",
        diagnostics);
    if (!parsed.ok) {
        mc::test_support::Fail("imported-module-surface fixture should parse successfully:\n" + diagnostics.Render());
    }

    mc::sema::Module imported_mem;
    mc::sema::TypeDeclSummary allocator_type;
    allocator_type.kind = mc::ast::Decl::Kind::kStruct;
    allocator_type.name = "Allocator";
    allocator_type.fields.push_back({"raw", mc::sema::NamedType("uintptr")});
    imported_mem.type_decls.push_back(std::move(allocator_type));
    imported_mem.functions.push_back({
        .name = "default_allocator",
        .return_types = {mc::sema::PointerType(mc::sema::NamedType("Allocator"))},
    });

    std::unordered_map<std::string, mc::sema::Module> imported_modules;
    imported_modules.emplace("mem", std::move(imported_mem));

    mc::sema::CheckOptions options;
    options.imported_modules = &imported_modules;
    const auto checked = mc::sema::CheckProgram(*parsed.source_file, "<imported-module-surface>", options, diagnostics);
    if (!checked.ok) {
        mc::test_support::Fail("imported module surfaces should qualify named types in options.imported_modules path:\n" + diagnostics.Render());
    }
}

void TestDuplicateBindingOrAssignDoesNotRecordInvalidFact() {
    mc::support::DiagnosticSink diagnostics;
    const auto parsed = ParseText(
        "func demo() i32 {\n"
        "    value, value = 1, 2\n"
        "    return 0\n"
        "}\n",
        "<duplicate-binding-or-assign>",
        diagnostics);
    if (!parsed.ok) {
        mc::test_support::Fail("duplicate binding-or-assignment fixture should parse successfully:\n" + diagnostics.Render());
    }

    const auto checked = mc::sema::CheckProgram(*parsed.source_file, "<duplicate-binding-or-assign>", diagnostics);
    if (checked.ok) {
        mc::test_support::Fail("duplicate binding-or-assignment should fail semantic checking");
    }
    if (diagnostics.Render().find("duplicate local binding: value") == std::string::npos) {
        mc::test_support::Fail("duplicate binding-or-assignment should report the duplicate local binding");
    }

    const auto& function = parsed.source_file->decls.front();
    if (function.body == nullptr || function.body->statements.empty() || function.body->statements.front() == nullptr) {
        mc::test_support::Fail("duplicate binding-or-assignment fixture should produce a function body statement");
    }

    if (mc::sema::FindBindingOrAssignFact(*checked.module, *function.body->statements.front()) != nullptr) {
        mc::test_support::Fail("duplicate binding-or-assignment should not record an invalid semantic fact");
    }
}

void TestColonAssignBindingDoesNotRecordBindingOrAssignFact() {
    mc::support::DiagnosticSink diagnostics;
    const auto parsed = ParseText(
        "func demo() i32 {\n"
        "    value := 1\n"
        "    return value\n"
        "}\n",
        "<colon-assign-binding>",
        diagnostics);
    if (!parsed.ok) {
        mc::test_support::Fail("colon-assign binding fixture should parse successfully:\n" + diagnostics.Render());
    }

    const auto checked = mc::sema::CheckProgram(*parsed.source_file, "<colon-assign-binding>", diagnostics);
    if (!checked.ok) {
        mc::test_support::Fail("colon-assign binding should pass semantic checking:\n" + diagnostics.Render());
    }

    const auto& function = parsed.source_file->decls.front();
    if (function.body == nullptr || function.body->statements.empty() || function.body->statements.front() == nullptr) {
        mc::test_support::Fail("colon-assign binding fixture should produce a function body statement");
    }

    if (function.body->statements.front()->kind != mc::ast::Stmt::Kind::kBinding) {
        mc::test_support::Fail("colon-assign binding should parse as an unambiguous binding statement");
    }

    if (mc::sema::FindBindingOrAssignFact(*checked.module, *function.body->statements.front()) != nullptr) {
        mc::test_support::Fail("colon-assign binding should not record a binding-or-assignment fact");
    }
}

void TestFloatToIntConstConversionHonorsInt64Bounds() {
    mc::support::DiagnosticSink diagnostics;
    const auto checked = CheckText(
        "const MIN_OK: i64 = (i64)(-9223372036854775808.0)\n"
        "const TOO_BIG: i64 = (i64)(9223372036854775808.0)\n"
        "\n"
        "func main() i32 {\n"
        "    return 0\n"
        "}\n",
        "<float-to-int-bounds>",
        diagnostics);

    if (checked.ok) {
        mc::test_support::Fail("float-to-int constant conversion should reject the exclusive int64 upper bound");
    }
    if (diagnostics.Render().find("top-level const initializer must be a compile-time constant") == std::string::npos) {
        mc::test_support::Fail("float-to-int constant conversion should reject out-of-range constants during const evaluation");
    }

    const auto* min_ok = mc::sema::FindGlobalSummary(*checked.module, "MIN_OK");
    if (min_ok == nullptr || min_ok->constant_values.empty() || !min_ok->constant_values.front().has_value() ||
        min_ok->constant_values.front()->kind != mc::sema::ConstValue::Kind::kInteger ||
        min_ok->constant_values.front()->integer_value != std::numeric_limits<std::int64_t>::min()) {
        mc::test_support::Fail("float-to-int constant conversion should still accept the inclusive int64 lower bound");
    }

    const auto* too_big = mc::sema::FindGlobalSummary(*checked.module, "TOO_BIG");
    if (too_big == nullptr || too_big->constant_values.empty() || too_big->constant_values.front().has_value()) {
        mc::test_support::Fail("float-to-int constant conversion should not materialize an out-of-range constant value");
    }
}

void TestImportedMutableGlobalDoesNotFoldAsConst() {
    mc::support::DiagnosticSink diagnostics;
    const auto parsed = ParseText(
        "import dep\n"
        "\n"
        "const COPY: i32 = dep.RUNTIME\n"
        "\n"
        "func main() i32 {\n"
        "    return 0\n"
        "}\n",
        "<imported-mutable-global>",
        diagnostics);
    if (!parsed.ok) {
        mc::test_support::Fail("imported mutable global fixture should parse successfully:\n" + diagnostics.Render());
    }

    mc::sema::Module imported_dep;
    mc::sema::ConstValue runtime_value;
    runtime_value.kind = mc::sema::ConstValue::Kind::kInteger;
    runtime_value.integer_value = 7;
    imported_dep.globals.push_back({
        .is_const = false,
        .names = {"RUNTIME"},
        .type = mc::sema::NamedType("i32"),
        .constant_values = {runtime_value},
    });

    std::unordered_map<std::string, mc::sema::Module> imported_modules;
    imported_modules.emplace("dep", std::move(imported_dep));

    mc::sema::CheckOptions options;
    options.imported_modules = &imported_modules;
    const auto checked = mc::sema::CheckProgram(*parsed.source_file, "<imported-mutable-global>", options, diagnostics);
    if (checked.ok) {
        mc::test_support::Fail("imported mutable globals should not be accepted as compile-time constants");
    }
    if (diagnostics.Render().find("top-level const initializer must be a compile-time constant") == std::string::npos) {
        mc::test_support::Fail("imported mutable globals should be rejected by const evaluation");
    }

    const auto* copy = mc::sema::FindGlobalSummary(*checked.module, "COPY");
    if (copy == nullptr || copy->constant_values.empty() || copy->constant_values.front().has_value()) {
        mc::test_support::Fail("rejected imported mutable globals should not materialize a constant value");
    }
}

void TestMalformedAggregateGlobalDoesNotStoreEmptyConstValue() {
    mc::support::DiagnosticSink diagnostics;
    const auto checked = CheckText(
        "struct Pair {\n"
        "    left: i32\n"
        "    right: i32\n"
        "}\n"
        "\n"
        "const BAD: Pair = { left: 1, left: 2 }\n"
        "\n"
        "func main() i32 {\n"
        "    return 0\n"
        "}\n",
        "<malformed-aggregate-global>",
        diagnostics);

    if (checked.ok) {
        mc::test_support::Fail("malformed aggregate globals should fail semantic checking");
    }
    if (diagnostics.Render().find("top-level const initializer must be a compile-time constant") == std::string::npos) {
        mc::test_support::Fail("malformed aggregate globals should report a constant-evaluation failure instead of dereferencing an empty optional");
    }

    const auto* bad = mc::sema::FindGlobalSummary(*checked.module, "BAD");
    if (bad == nullptr || bad->constant_values.empty() || bad->constant_values.front().has_value()) {
        mc::test_support::Fail("malformed aggregate globals should not record a constant value");
    }
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        mc::test_support::Fail("expected source root argument");
    }

    const std::filesystem::path source_root = argv[1];
    const std::filesystem::path fixture_dir = source_root / "tests/compiler/sema";
    const std::vector<FixtureCase> explicit_fixtures = {
        {"import_root_ok_main.mc", "import_root_ok_main.sema.txt", {"tests/compiler/sema/import_roots"}, true},
        {"switch_qualified_const_ok.mc", "switch_qualified_const_ok.sema.txt", {"tests/compiler/sema/import_roots"}, true},
        {"internal_same_package/main.mc", "internal_same_package/main.sema.txt", {}, true, "pkg", {{"internal_same_package", "pkg"}}},
        {"internal_cross_package/pkg_a/main.mc", "internal_cross_package/pkg_a/main.errors.txt", {"tests/compiler/sema/internal_cross_package/pkg_b"}, false, "pkg_a", {{"internal_cross_package/pkg_a", "pkg_a"}, {"internal_cross_package/pkg_b", "pkg_b"}}},
    };

    std::unordered_set<std::string> explicit_sources;
    explicit_sources.reserve(explicit_fixtures.size());
    for (const auto& fixture : explicit_fixtures) {
        explicit_sources.insert(fixture.source_name);
        RunFixture(source_root, fixture_dir, fixture);
    }

    const auto discovered = mc::test_support::DiscoverAdjacentFixtures(
        fixture_dir,
        ".sema.txt",
        ".errors.txt",
        mc::test_support::FixtureDiscoveryMode::kExistingExpectationOnly);

    for (const auto& discovered_fixture : discovered) {
        if (explicit_sources.contains(discovered_fixture.source_name)) {
            continue;
        }
        RunFixture(source_root, fixture_dir, BuildFixtureCase(discovered_fixture));
    }

    TestImportedModuleSurfaceQualifiesNamedTypes();
    TestDuplicateBindingOrAssignDoesNotRecordInvalidFact();
    TestColonAssignBindingDoesNotRecordBindingOrAssignFact();
    TestFloatToIntConstConversionHonorsInt64Bounds();
    TestImportedMutableGlobalDoesNotFoldAsConst();
    TestMalformedAggregateGlobalDoesNotStoreEmptyConstValue();

    return 0;
}