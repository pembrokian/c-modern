#include <filesystem>
#include <string>

#include "tests/support/process_utils.h"
#include "tests/tool/tool_suite_common.h"
#include "tests/tool/tool_workflow_suite_internal.h"

namespace {

using mc::test_support::ExpectOutputContains;
using mc::test_support::Fail;
using mc::test_support::RunCommandCapture;
using mc::test_support::WriteFile;
using namespace mc::tool_tests;
using mc::tool_tests::BuildProjectTargetAndExpectFailure;

void TestUnknownTargetListsAvailableTargets(const std::filesystem::path& binary_root,
                                            const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "unknown_target_project";
    std::filesystem::remove_all(project_root);
    WriteFile(project_root / "build.toml",
              "schema = 1\n"
              "project = \"phase19-unknown-target\"\n"
              "default = \"app\"\n"
              "\n"
              "[targets.app]\n"
              "kind = \"exe\"\n"
              "package = \"phase19-unknown-target\"\n"
              "root = \"src/main.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"hosted\"\n"
              "\n"
              "[targets.app.search_paths]\n"
              "modules = [\"src\"]\n"
              "\n"
              "[targets.app.runtime]\n"
              "startup = \"default\"\n"
              "\n"
              "[targets.tool]\n"
              "kind = \"exe\"\n"
              "package = \"phase19-unknown-target\"\n"
              "root = \"src/tool.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"hosted\"\n"
              "\n"
              "[targets.tool.search_paths]\n"
              "modules = [\"src\"]\n"
              "\n"
              "[targets.tool.runtime]\n"
              "startup = \"default\"\n");
    WriteFile(project_root / "src/main.mc", "func main() i32 { return 0 }\n");
    WriteFile(project_root / "src/tool.mc", "func main() i32 { return 0 }\n");

    const auto [outcome, output] = RunCommandCapture({mc_path.generic_string(),
                                                      "build",
                                                      "--project",
                                                      (project_root / "build.toml").generic_string(),
                                                      "--target",
                                                      "missing"},
                                                     binary_root / "unknown_target_output.txt",
                                                     "unknown target build");
    if (!outcome.exited || outcome.exit_code == 0) {
        Fail("unknown target should fail project selection");
    }
    ExpectOutputContains(output,
                         "unknown target in project file: missing; available targets: app, tool",
                         "unknown target should list the available targets");
    ExpectOutputContains(output,
                         "note: available targets: app, tool",
                         "unknown target should emit available-target note");
    ExpectOutputContains(output,
                         "note: pass --target <name> to select one target explicitly",
                         "unknown target should emit explicit target guidance");
}

void TestMissingDefaultTargetFails(const std::filesystem::path& binary_root,
                                   const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "no_default_project";
    std::filesystem::remove_all(project_root);
    WriteFile(project_root / "build.toml",
              "schema = 1\n"
              "project = \"phase7-no-default\"\n"
              "\n"
              "[targets.first]\n"
              "kind = \"exe\"\n"
              "package = \"phase7-no-default\"\n"
              "root = \"src/main.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"hosted\"\n"
              "\n"
              "[targets.first.search_paths]\n"
              "modules = [\"src\"]\n"
              "\n"
              "[targets.first.runtime]\n"
              "startup = \"default\"\n"
              "\n"
              "[targets.second]\n"
              "kind = \"exe\"\n"
              "package = \"phase7-no-default\"\n"
              "root = \"src/alt.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"hosted\"\n"
              "\n"
              "[targets.second.search_paths]\n"
              "modules = [\"src\"]\n"
              "\n"
              "[targets.second.runtime]\n"
              "startup = \"default\"\n");
    WriteFile(project_root / "src/main.mc", "func main() i32 { return 0 }\n");
    WriteFile(project_root / "src/alt.mc", "func main() i32 { return 0 }\n");

    const auto [missing_default_outcome, missing_default_output] = RunCommandCapture({mc_path.generic_string(),
                                                                                      "build",
                                                                                      "--project",
                                                                                      (project_root / "build.toml").generic_string()},
                                                                                     binary_root / "missing_default_output.txt",
                                                                                     "missing default target");
    if (!missing_default_outcome.exited || missing_default_outcome.exit_code == 0) {
        Fail("project without default target should fail when no --target is provided");
    }
    ExpectOutputContains(missing_default_output,
                         "project file does not declare a default target; available targets: first, second; pass --target <name>",
                         "missing default target diagnostic");
    ExpectOutputContains(missing_default_output,
                         "note: available targets: first, second",
                         "missing default should emit available-target note");
    ExpectOutputContains(missing_default_output,
                         "note: pass --target <name> to select one target explicitly",
                         "missing default should emit explicit target guidance");

    const std::filesystem::path explicit_target_build_dir = binary_root / "explicit_target_build";
    std::filesystem::remove_all(explicit_target_build_dir);

    const auto [explicit_target_outcome, explicit_target_output] = RunCommandCapture({mc_path.generic_string(),
                                                                                      "build",
                                                                                      "--project",
                                                                                      (project_root / "build.toml").generic_string(),
                                                                                      "--target",
                                                                                      "second",
                                                                                      "--build-dir",
                                                                                      explicit_target_build_dir.generic_string()},
                                                                                     binary_root / "explicit_target_output.txt",
                                                                                     "explicit target build");
    if (!explicit_target_outcome.exited || explicit_target_outcome.exit_code != 0) {
        Fail("project build with explicit --target should succeed:\n" + explicit_target_output);
    }
}

void TestProjectMissingImportRootFails(const std::filesystem::path& source_root,
                                       const std::filesystem::path& binary_root,
                                       const std::filesystem::path& mc_path) {
    const std::filesystem::path fixture_root = source_root / "tests/tool/phase7_missing_root_project";
    const std::filesystem::path output_path = binary_root / "missing_root_output.txt";
    const auto [outcome, output] = RunCommandCapture({mc_path.generic_string(),
                                                      "check",
                                                      "--project",
                                                      (fixture_root / "build.toml").generic_string()},
                                                     output_path,
                                                     "missing root check");
    if (!outcome.exited || outcome.exit_code == 0) {
        Fail("phase7 missing import root check should fail");
    }
    ExpectOutputContains(output, "unable to resolve import module: hidden", "missing import root diagnostic");
}

void TestProjectAmbiguousImportFails(const std::filesystem::path& source_root,
                                     const std::filesystem::path& binary_root,
                                     const std::filesystem::path& mc_path) {
    const std::filesystem::path fixture_root = source_root / "tests/tool/phase7_ambiguous_project";
    const std::filesystem::path output_path = binary_root / "ambiguous_output.txt";
    const auto [outcome, output] = RunCommandCapture({mc_path.generic_string(),
                                                      "check",
                                                      "--project",
                                                      (fixture_root / "build.toml").generic_string()},
                                                     output_path,
                                                     "ambiguous import check");
    if (!outcome.exited || outcome.exit_code == 0) {
        Fail("phase7 ambiguous import check should fail");
    }
    ExpectOutputContains(output, "ambiguous import module 'helper' matched multiple roots", "ambiguous import diagnostic");
}

void TestDuplicateModuleRootFailsEarly(const std::filesystem::path& binary_root,
                                       const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "duplicate_root_project";
    std::filesystem::remove_all(project_root);
    WriteFile(project_root / "build.toml",
              "schema = 1\n"
              "project = \"phase13-duplicate-root\"\n"
              "default = \"app\"\n"
              "\n"
              "[targets.app]\n"
              "kind = \"exe\"\n"
              "package = \"phase13-duplicate-root\"\n"
              "root = \"src/main.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"hosted\"\n"
              "\n"
              "[targets.app.search_paths]\n"
              "modules = [\"src\", \"./src\"]\n"
              "\n"
              "[targets.app.runtime]\n"
              "startup = \"default\"\n");
    WriteFile(project_root / "src/main.mc", "func main() i32 { return 0 }\n");

    const auto [outcome, output] = RunCommandCapture({mc_path.generic_string(),
                                                      "build",
                                                      "--project",
                                                      (project_root / "build.toml").generic_string()},
                                                     binary_root / "duplicate_root_output.txt",
                                                     "duplicate module root build");
    if (!outcome.exited || outcome.exit_code == 0) {
        Fail("project with duplicate module roots should fail during project validation");
    }
    ExpectOutputContains(output,
                         "target 'app' declares duplicate module search path:",
                         "duplicate module roots should produce a strict project diagnostic");
}

void TestDuplicateTargetRootsFailEarly(const std::filesystem::path& binary_root,
                                       const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "duplicate_target_root_project";
    std::filesystem::remove_all(project_root);
    WriteFile(project_root / "build.toml",
              "schema = 1\n"
              "project = \"phase13-duplicate-target-root\"\n"
              "default = \"first\"\n"
              "\n"
              "[targets.first]\n"
              "kind = \"exe\"\n"
              "package = \"phase13-duplicate-target-root\"\n"
              "root = \"src/main.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"hosted\"\n"
              "\n"
              "[targets.first.search_paths]\n"
              "modules = [\"src\"]\n"
              "\n"
              "[targets.first.runtime]\n"
              "startup = \"default\"\n"
              "\n"
              "[targets.second]\n"
              "kind = \"exe\"\n"
              "package = \"phase13-duplicate-target-root\"\n"
              "root = \"./src/main.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"hosted\"\n"
              "\n"
              "[targets.second.search_paths]\n"
              "modules = [\"src\"]\n"
              "\n"
              "[targets.second.runtime]\n"
              "startup = \"default\"\n");
    WriteFile(project_root / "src/main.mc", "func main() i32 { return 0 }\n");

    const auto [outcome, output] = RunCommandCapture({mc_path.generic_string(),
                                                      "build",
                                                      "--project",
                                                      (project_root / "build.toml").generic_string()},
                                                     binary_root / "duplicate_target_root_output.txt",
                                                     "duplicate target roots build");
    if (!outcome.exited || outcome.exit_code == 0) {
        Fail("project with duplicate target roots should fail during target validation");
    }
    ExpectOutputContains(output,
                         "declare the same root module:",
                         "duplicate target roots should produce a deterministic graph diagnostic");
}

void TestExecutableTargetRejectsNonStaticLibraryLink(const std::filesystem::path& binary_root,
                                                     const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "invalid_staticlib_link_project";
    std::filesystem::remove_all(project_root);
    WriteFile(project_root / "build.toml",
              "schema = 1\n"
              "project = \"phase29-invalid-staticlib-link\"\n"
              "default = \"app\"\n"
              "\n"
              "[targets.lib]\n"
              "kind = \"exe\"\n"
              "package = \"phase29-invalid-staticlib-link\"\n"
              "root = \"src/lib_main.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"hosted\"\n"
              "\n"
              "[targets.lib.search_paths]\n"
              "modules = [\"src\"]\n"
              "\n"
              "[targets.lib.runtime]\n"
              "startup = \"default\"\n"
              "\n"
              "[targets.app]\n"
              "kind = \"exe\"\n"
              "package = \"phase29-invalid-staticlib-link\"\n"
              "root = \"src/main.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"hosted\"\n"
              "links = [\"lib\"]\n"
              "\n"
              "[targets.app.search_paths]\n"
              "modules = [\"src\"]\n"
              "\n"
              "[targets.app.runtime]\n"
              "startup = \"default\"\n");
    WriteFile(project_root / "src/lib_main.mc",
              "func helper() i32 {\n"
              "    return 7\n"
              "}\n"
              "\n"
              "func main() i32 {\n"
              "    return helper()\n"
              "}\n");
    WriteFile(project_root / "src/main.mc",
              "import lib_main\n"
              "\n"
              "func main() i32 {\n"
              "    return lib_main.helper()\n"
              "}\n");

    const std::filesystem::path build_dir = binary_root / "invalid_staticlib_link_build";
    std::filesystem::remove_all(build_dir);
    const std::string output = BuildProjectTargetAndExpectFailure(mc_path,
                                                                  project_root / "build.toml",
                                                                  build_dir,
                                                                  "app",
                                                                  "invalid_staticlib_link_output.txt",
                                                                  "invalid executable-to-executable link build");
    ExpectOutputContains(output,
                         "target 'app' can only link static libraries, but 'lib' has kind 'exe'",
                         "non-static linked targets should fail with a clear project-graph diagnostic");
}

void TestUnsupportedFreestandingBootstrapTargetEmitsNotes(const std::filesystem::path& binary_root,
                                                          const std::filesystem::path& mc_path) {
    const std::filesystem::path unsupported_target_root = binary_root / "unsupported_freestanding_target_project";
    std::filesystem::remove_all(unsupported_target_root);
    WriteFile(unsupported_target_root / "build.toml",
              "schema = 1\n"
              "project = \"phase223-unsupported-freestanding-target\"\n"
              "default = \"kernel\"\n"
              "\n"
              "[targets.kernel]\n"
              "kind = \"exe\"\n"
              "package = \"phase223-unsupported-freestanding-target\"\n"
              "root = \"src/main.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"freestanding\"\n"
              "target = \"bogus-target\"\n"
              "\n"
              "[targets.kernel.search_paths]\n"
              "modules = [\"src\"]\n"
              "\n"
              "[targets.kernel.runtime]\n"
              "startup = \"kernel_entry\"\n");
    WriteFile(unsupported_target_root / "src/main.mc", "func main() i32 { return 0 }\n");

    const std::string unsupported_target_output = BuildProjectTargetAndExpectFailure(mc_path,
                                                                                     unsupported_target_root / "build.toml",
                                                                                     binary_root / "unsupported_freestanding_target_build",
                                                                                     "kernel",
                                                                                     "unsupported_freestanding_target_output.txt",
                                                                                     "unsupported freestanding target build");
    ExpectOutputContains(unsupported_target_output,
                         "target 'kernel' requests unsupported bootstrap target: bogus-target",
                         "freestanding target selection should reject unsupported bootstrap targets");
    ExpectOutputContains(unsupported_target_output,
                         "note: supported bootstrap targets:",
                         "unsupported bootstrap target should list supported values");
    ExpectOutputContains(unsupported_target_output,
                         "note: set [targets.kernel] target = \"",
                         "unsupported bootstrap target should tell the user which manifest field to change");
}

void TestMissingFreestandingBootstrapTargetEmitsNotes(const std::filesystem::path& binary_root,
                                                      const std::filesystem::path& mc_path) {
    const std::filesystem::path missing_target_root = binary_root / "missing_freestanding_target_project";
    std::filesystem::remove_all(missing_target_root);
    WriteFile(missing_target_root / "build.toml",
              "schema = 1\n"
              "project = \"phase223a-missing-freestanding-target\"\n"
              "default = \"kernel\"\n"
              "\n"
              "[targets.kernel]\n"
              "kind = \"exe\"\n"
              "package = \"phase223a-missing-freestanding-target\"\n"
              "root = \"src/main.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"freestanding\"\n"
              "\n"
              "[targets.kernel.search_paths]\n"
              "modules = [\"src\"]\n"
              "\n"
              "[targets.kernel.runtime]\n"
              "startup = \"kernel_entry\"\n");
    WriteFile(missing_target_root / "src/main.mc", "func main() i32 { return 0 }\n");

    const std::string missing_target_output = BuildProjectTargetAndExpectFailure(mc_path,
                                                                                 missing_target_root / "build.toml",
                                                                                 binary_root / "missing_freestanding_target_build",
                                                                                 "kernel",
                                                                                 "missing_freestanding_target_output.txt",
                                                                                 "missing freestanding target build");
    ExpectOutputContains(missing_target_output,
                         "target 'kernel' must declare an explicit freestanding target",
                         "missing freestanding target should fail during project validation");
    ExpectOutputContains(missing_target_output,
                         "note: supported bootstrap targets:",
                         "missing freestanding target should list supported bootstrap targets");
    ExpectOutputContains(missing_target_output,
                         "note: set [targets.kernel] target = \"",
                         "missing freestanding target should tell the user which manifest field to add");
}

void TestSelectorRegistrationIntegrityFollowsDescriptors(const std::filesystem::path& source_root,
                                                         const std::filesystem::path& binary_root) {
    const std::filesystem::path selector_script = source_root / "tools/select_tests.py";
    const std::filesystem::path output_path = binary_root / "select_tests_registration_integrity_output.txt";

    const auto [outcome, output] = RunCommandCapture({"/usr/bin/env",
                                                      "python3",
                                                      selector_script.generic_string(),
                                                      "--source-root",
                                                      source_root.generic_string(),
                                                      "--changed",
                                                      "tests/tool/workflow/kernel-reset-lane/case.toml",
                                                      "--changed",
                                                      "tests/tool/real_projects/issue-rollup/case.toml"},
                                                     output_path,
                                                     "selector registration integrity check");
    if (!outcome.exited || outcome.exit_code != 0) {
        Fail("select_tests.py should accept the discovered workflow and real-project descriptors:\n" + output);
    }

    ExpectOutputContains(output,
                         "mc_tool_workflow_kernel_reset_lane_unit",
                         "selector should keep the kernel reset lane workflow case registered");
    ExpectOutputContains(output,
                         "mc_tool_real_project_issue_rollup_unit",
                         "selector should keep the issue-rollup real-project case registered");
}

}  // namespace

namespace mc::tool_tests {

void RunWorkflowProjectValidationSuite(const std::filesystem::path& source_root,
                                       const std::filesystem::path& binary_root,
                                       const std::filesystem::path& mc_path) {
    TestUnknownTargetListsAvailableTargets(binary_root, mc_path);
    TestMissingDefaultTargetFails(binary_root, mc_path);
    TestProjectMissingImportRootFails(source_root, binary_root, mc_path);
    TestProjectAmbiguousImportFails(source_root, binary_root, mc_path);
    TestDuplicateModuleRootFailsEarly(binary_root, mc_path);
    TestDuplicateTargetRootsFailEarly(binary_root, mc_path);
    TestExecutableTargetRejectsNonStaticLibraryLink(binary_root, mc_path);
    TestUnsupportedFreestandingBootstrapTargetEmitsNotes(binary_root, mc_path);
    TestMissingFreestandingBootstrapTargetEmitsNotes(binary_root, mc_path);
    TestSelectorRegistrationIntegrityFollowsDescriptors(source_root, binary_root);
}

}  // namespace mc::tool_tests
