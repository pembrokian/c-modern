#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "tests/support/process_utils.h"
#include "tests/tool/tool_suite_common.h"
#include "tests/tool/tool_workflow_suite_internal.h"

namespace {

using mc::test_support::CommandOutcome;
using mc::test_support::ExpectOutputContains;
using mc::test_support::Fail;
using mc::test_support::RunCommandCapture;
using mc::test_support::WriteFile;
using namespace mc::tool_tests;

std::pair<CommandOutcome, std::string> RunProjectTestCommand(const std::filesystem::path& mc_path,
                                                             const std::filesystem::path& project_path,
                                                             const std::filesystem::path& build_dir,
                                                             const std::filesystem::path& output_path,
                                                             const std::string& context,
                                                             std::string_view target_name = {}) {
    std::vector<std::string> command = {mc_path.generic_string(),
                                        "test",
                                        "--project",
                                        project_path.generic_string(),
                                        "--build-dir",
                                        build_dir.generic_string()};
    if (!target_name.empty()) {
        command.push_back("--target");
        command.push_back(std::string(target_name));
    }
    return RunCommandCapture(command, output_path, context);
}

void TestProjectTestCommandSucceeds(const std::filesystem::path& binary_root,
                                    const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "test_success_project";
    const std::filesystem::path project_path = WriteTestProject(
        project_root,
        "func main() i32 {\n"
        "    return 0\n"
        "}\n");
    WriteFile(project_root / "tests/alpha_test.mc",
              "func test_alpha_pass() *i32 {\n"
              "    return nil\n"
              "}\n");
    WriteFile(project_root / "tests/beta_test.mc",
              "func test_beta_pass() *i32 {\n"
              "    return nil\n"
              "}\n");
    WriteFile(project_root / "tests/compiler_manifest.txt",
              "check-pass regressions/check_ok.mc\n"
              "check-fail regressions/check_fail.mc regressions/check_fail.errors.txt\n"
              "run-output regressions/run_ok.mc 5 regressions/run_ok.stdout.txt\n"
              "mir regressions/mir_ok.mc regressions/mir_ok.mir.txt\n");
    WriteFile(project_root / "tests/regressions/check_ok.mc",
              "func helper(value: i32) i32 {\n"
              "    return value\n"
              "}\n");
    WriteFile(project_root / "tests/regressions/check_fail.mc",
              "import missing\n"
              "\n"
              "func main() i32 {\n"
              "    return 0\n"
              "}\n");
    WriteFile(project_root / "tests/regressions/check_fail.errors.txt",
              "unable to resolve import module: missing\n");
    WriteFile(project_root / "tests/regressions/run_ok.mc",
              "import io\n"
              "\n"
              "func main() i32 {\n"
              "    if io.write_line(\"phase7 run output\") != 0 {\n"
              "        return 1\n"
              "    }\n"
              "    return 5\n"
              "}\n");
    WriteFile(project_root / "tests/regressions/run_ok.stdout.txt",
              "phase7 run output\n");
    WriteFile(project_root / "tests/regressions/mir_ok.mc",
              "distinct UserId = i32\n"
              "\n"
              "func promote(raw: i32) UserId {\n"
              "    return (UserId)(raw)\n"
              "}\n");
    WriteFile(project_root / "tests/regressions/mir_ok.mir.txt",
              "Module\n"
              "  TypeDecl kind=distinct name=UserId\n"
              "    AliasedType=i32\n"
              "  Function name=promote returns=[UserId]\n"
              "    Local name=raw type=i32 param readonly\n"
              "    Block label=entry0\n"
              "      load_local %v0:i32 target=raw\n"
              "      convert_distinct %v1:UserId target=UserId operands=[%v0]\n"
              "      terminator return values=[%v1]\n");

    const std::filesystem::path build_dir = binary_root / "test_success_build";
    std::filesystem::remove_all(build_dir);
    const auto [outcome, output] = RunProjectTestCommand(mc_path,
                                                         project_path,
                                                         build_dir,
                                                         build_dir / "test_success_output.txt",
                                                         "test success command");
    if (!outcome.exited || outcome.exit_code != 0) {
        Fail("mc test success project should pass:\n" + output);
    }

    ExpectOutputContains(output, "testing target app", "mc test should announce the target under test");
    ExpectOutputContains(output,
                         "ordinary tests target app: 2 cases, mode=checked, timeout=5000 ms",
                         "mc test should print the ordinary test scope");
    ExpectOutputContains(output, "PASS alpha_test.test_alpha_pass", "ordinary tests should report passing cases");
    ExpectOutputContains(output, "PASS beta_test.test_beta_pass", "ordinary tests should be discovered deterministically");
    ExpectOutputContains(output, "2 tests, 2 passed, 0 failed", "ordinary test summary should be printed");
    ExpectOutputContains(output,
                         "PASS ordinary tests for target app (2 cases)",
                         "ordinary tests should print a target-scoped verdict");
    ExpectOutputContains(output,
                         "compiler regressions target app: 4 cases, timeout=5000 ms",
                         "compiler regressions should print their scope");
    ExpectOutputContains(output,
                         "PASS check-pass " + (project_root / "tests/regressions/check_ok.mc").generic_string(),
                         "compiler regression check-pass should run");
    ExpectOutputContains(output,
                         "PASS check-fail " + (project_root / "tests/regressions/check_fail.mc").generic_string(),
                         "compiler regression check-fail should run");
    ExpectOutputContains(output,
                         "PASS run-output " + (project_root / "tests/regressions/run_ok.mc").generic_string(),
                         "compiler regression run-output should run");
    ExpectOutputContains(output,
                         "PASS mir " + (project_root / "tests/regressions/mir_ok.mc").generic_string(),
                         "compiler regression MIR fixture should run");
    ExpectOutputContains(output,
                         "PASS compiler regressions for target app (4 cases)",
                         "compiler regressions should print a target-scoped verdict");
    ExpectOutputContains(output,
                         "PASS target app",
                         "mc test should print the overall target verdict");
}

void TestProjectTestCommandFailsOnOrdinaryFailure(const std::filesystem::path& binary_root,
                                                  const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "test_failure_project";
    const std::filesystem::path project_path = WriteTestProject(
        project_root,
        "func main() i32 {\n"
        "    return 0\n"
        "}\n");
    WriteFile(project_root / "tests/failing_test.mc",
              "func test_failure() *i32 {\n"
              "    return (*i32)((uintptr)(1))\n"
              "}\n");

    const std::filesystem::path build_dir = binary_root / "test_failure_build";
    std::filesystem::remove_all(build_dir);
    const auto [outcome, output] = RunProjectTestCommand(mc_path,
                                                         project_path,
                                                         build_dir,
                                                         build_dir / "test_failure_output.txt",
                                                         "test failure command");
    if (!outcome.exited || outcome.exit_code == 0) {
        Fail("mc test failure project should return a non-zero exit status:\n" + output);
    }

    ExpectOutputContains(output,
                         "ordinary tests target app: 1 cases, mode=checked, timeout=5000 ms",
                         "ordinary failure should still print the ordinary test scope");
    ExpectOutputContains(output, "FAIL failing_test.test_failure", "ordinary test failure should be reported");
    ExpectOutputContains(output, "1 tests, 0 passed, 1 failed", "ordinary test failure summary should be printed");
    ExpectOutputContains(output,
                         "FAIL ordinary tests for target app (1 cases)",
                         "ordinary failure should print a target-scoped verdict");
    ExpectOutputContains(output,
                         "FAIL target app",
                         "mc test should print the overall target failure verdict");
}

void TestDisabledTestTargetListsEnabledTargets(const std::filesystem::path& binary_root,
                                               const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "disabled_test_target_project";
    std::filesystem::remove_all(project_root);
    WriteFile(project_root / "build.toml",
              "schema = 1\n"
              "project = \"phase19-disabled-test-target\"\n"
              "default = \"app\"\n"
              "\n"
              "[targets.app]\n"
              "kind = \"exe\"\n"
              "package = \"phase19-disabled-test-target\"\n"
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
              "[targets.unit]\n"
              "kind = \"exe\"\n"
              "package = \"phase19-disabled-test-target\"\n"
              "root = \"src/unit.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"hosted\"\n"
              "\n"
              "[targets.unit.search_paths]\n"
              "modules = [\"src\"]\n"
              "\n"
              "[targets.unit.runtime]\n"
              "startup = \"default\"\n"
              "\n"
              "[targets.unit.tests]\n"
              "enabled = true\n"
              "roots = [\"tests\"]\n"
              "mode = \"checked\"\n"
              "timeout_ms = 5000\n");
    WriteFile(project_root / "src/main.mc", "func main() i32 { return 0 }\n");
    WriteFile(project_root / "src/unit.mc", "func main() i32 { return 0 }\n");
    WriteFile(project_root / "tests/sample_test.mc",
              "func test_ok() *i32 {\n"
              "    return nil\n"
              "}\n");

    const auto [outcome, output] = RunProjectTestCommand(mc_path,
                                                         project_root / "build.toml",
                                                         binary_root / "disabled_test_target_build",
                                                         binary_root / "disabled_test_target_output.txt",
                                                         "disabled test target selection",
                                                         "app");
    if (!outcome.exited || outcome.exit_code == 0) {
        Fail("mc test should fail when the selected target has tests disabled");
    }
    ExpectOutputContains(output,
                         "tests are not enabled for target 'app'; enabled test targets: unit",
                         "disabled test target diagnostic should list enabled test targets");
}

void TestProjectTestTimeoutFailsDeterministically(const std::filesystem::path& binary_root,
                                                  const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "test_timeout_project";
    std::filesystem::remove_all(project_root);
    WriteFile(project_root / "build.toml",
              "schema = 1\n"
              "project = \"phase13-test-timeout\"\n"
              "default = \"app\"\n"
              "\n"
              "[targets.app]\n"
              "kind = \"exe\"\n"
              "package = \"phase13-test-timeout\"\n"
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
              "[targets.app.tests]\n"
              "enabled = true\n"
              "roots = [\"tests\"]\n"
              "mode = \"checked\"\n"
              "timeout_ms = 100\n");
    WriteFile(project_root / "src/main.mc", "func main() i32 { return 0 }\n");
    WriteFile(project_root / "tests/hang_test.mc",
              "func test_hang() *i32 {\n"
              "    while true {\n"
              "    }\n"
              "    return nil\n"
              "}\n");

    const std::filesystem::path build_dir = binary_root / "test_timeout_build";
    std::filesystem::remove_all(build_dir);
    std::filesystem::create_directories(build_dir);

    const auto [outcome, output] = RunProjectTestCommand(mc_path,
                                                         project_root / "build.toml",
                                                         build_dir,
                                                         build_dir / "timeout_test_output.txt",
                                                         "timeout test command");
    if (!outcome.exited || outcome.exit_code == 0) {
        Fail("mc test with a hanging ordinary test should fail");
    }
    ExpectOutputContains(output,
                         "ordinary tests target app: 1 cases, mode=checked, timeout=100 ms",
                         "ordinary timeout should still print the ordinary test scope");
    ExpectOutputContains(output,
                         "TIMEOUT ordinary tests for target app after 100 ms",
                         "ordinary test timeout should be reported deterministically");
    ExpectOutputContains(output,
                         "FAIL ordinary tests for target app (timeout)",
                         "ordinary timeout should print a target-scoped failure verdict");
    ExpectOutputContains(output,
                         "FAIL target app",
                         "mc test should print the overall target failure verdict on timeout");
}

void TestDuplicateOrdinaryTestModuleNamesExplainBootstrapRule(const std::filesystem::path& binary_root,
                                                              const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "duplicate_ordinary_test_module_project";
    const std::filesystem::path project_path = WriteTestProject(
        project_root,
        "func main() i32 {\n"
        "    return 0\n"
        "}\n");
    WriteFile(project_root / "tests/alpha/shared_test.mc",
              "func test_alpha() *i32 {\n"
              "    return nil\n"
              "}\n");
    WriteFile(project_root / "tests/beta/shared_test.mc",
              "func test_beta() *i32 {\n"
              "    return nil\n"
              "}\n");

    const std::filesystem::path build_dir = binary_root / "duplicate_ordinary_test_module_build";
    std::filesystem::remove_all(build_dir);
    const auto [outcome, output] = RunProjectTestCommand(mc_path,
                                                         project_path,
                                                         build_dir,
                                                         build_dir / "duplicate_ordinary_test_module_output.txt",
                                                         "duplicate ordinary test module names");
    if (!outcome.exited || outcome.exit_code == 0) {
        Fail("mc test should reject duplicate ordinary test module stems across discovered test roots:\n" + output);
    }
    ExpectOutputContains(output,
                         "duplicate ordinary test module name discovered: shared_test",
                         "duplicate ordinary test module names should mention the conflicting module stem");
    ExpectOutputContains(output,
                         "bootstrap ordinary test discovery requires globally unique file stems across all configured test roots",
                         "duplicate ordinary test module names should explain the bootstrap uniqueness rule");
}

void TestGeneratedOrdinaryRunnerRequiresRepositoryStdlibRoot(const std::filesystem::path& binary_root,
                                                             const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "external_generated_runner_project";
    const std::filesystem::path project_path = WriteTestProject(
        project_root,
        "func main() i32 {\n"
        "    return 0\n"
        "}\n");
    WriteFile(project_root / "tests/sample_test.mc",
              "func test_sample() *i32 {\n"
              "    return nil\n"
              "}\n");

    const std::filesystem::path build_dir = std::filesystem::temp_directory_path() / "mc_phase127_external_generated_runner_build";
    std::filesystem::remove_all(build_dir);
    const auto [outcome, output] = RunProjectTestCommand(mc_path,
                                                         project_path,
                                                         build_dir,
                                                         build_dir / "external_generated_runner_output.txt",
                                                         "generated ordinary runner repository root discovery");
    std::filesystem::remove_all(build_dir);
    if (!outcome.exited || outcome.exit_code == 0) {
        Fail("mc test should fail explicitly when generated ordinary test runner stdlib discovery cannot find the repository root:\n" + output);
    }
    ExpectOutputContains(output,
                         "unable to discover repository root for generated ordinary test runner imports from",
                         "generated ordinary test runner should fail with an explicit repository-root discovery diagnostic");
    ExpectOutputContains(output,
                         "repository stdlib root is required",
                         "generated ordinary test runner should explain why repository-root discovery matters");
}

}  // namespace

namespace mc::tool_tests {

void RunWorkflowTestCommandSuite(const std::filesystem::path& binary_root,
                                 const std::filesystem::path& mc_path) {
    TestProjectTestCommandSucceeds(binary_root, mc_path);
    TestProjectTestCommandFailsOnOrdinaryFailure(binary_root, mc_path);
    TestDisabledTestTargetListsEnabledTargets(binary_root, mc_path);
    TestProjectTestTimeoutFailsDeterministically(binary_root, mc_path);
    TestDuplicateOrdinaryTestModuleNamesExplainBootstrapRule(binary_root, mc_path);
    TestGeneratedOrdinaryRunnerRequiresRepositoryStdlibRoot(binary_root, mc_path);
}

}  // namespace mc::tool_tests
