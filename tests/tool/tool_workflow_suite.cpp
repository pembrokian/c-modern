#include <filesystem>
#include <string>

#include "tests/support/process_utils.h"
#include "tests/tool/tool_suite_common.h"

namespace {

using mc::test_support::ExpectOutputContains;
using mc::test_support::Fail;
using mc::test_support::RunCommandCapture;
using mc::test_support::WriteFile;
using namespace mc::tool_tests;
using mc::tool_tests::BuildProjectTargetAndExpectFailure;
using mc::tool_tests::WriteTestProject;

void TestHelpMentionsRun(const std::filesystem::path& binary_root,
                         const std::filesystem::path& mc_path) {
    const auto [outcome, output] = RunCommandCapture({mc_path.generic_string(), "--help"},
                                                     binary_root / "help_output.txt",
                                                     "help command");
    if (!outcome.exited || outcome.exit_code != 0) {
        Fail("mc --help should succeed");
    }
    ExpectOutputContains(output, "mc run", "help text should mention mc run");
    ExpectOutputContains(output, "mc test", "help text should mention mc test");
    ExpectOutputContains(output, "--mode <name>", "help text should mention mode overrides");
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
    const auto [outcome, output] = RunCommandCapture({mc_path.generic_string(),
                                                      "test",
                                                      "--project",
                                                      project_path.generic_string(),
                                                      "--build-dir",
                                                      build_dir.generic_string()},
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
              "import testing\n"
              "\n"
              "func test_failure() *i32 {\n"
              "    return testing.fail()\n"
              "}\n");

    const std::filesystem::path build_dir = binary_root / "test_failure_build";
    std::filesystem::remove_all(build_dir);
    const auto [outcome, output] = RunCommandCapture({mc_path.generic_string(),
                                                      "test",
                                                      "--project",
                                                      project_path.generic_string(),
                                                      "--build-dir",
                                                      build_dir.generic_string()},
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

void TestMixedDirectSourceAndProjectOptionsFail(const std::filesystem::path& binary_root,
                                                const std::filesystem::path& mc_path) {
    const std::filesystem::path source_path = binary_root / "mixed_mode.mc";
    WriteFile(source_path,
              "func main() i32 {\n"
              "    return 0\n"
              "}\n");

    const auto [outcome, output] = RunCommandCapture({mc_path.generic_string(),
                                                      "check",
                                                      source_path.generic_string(),
                                                      "--target",
                                                      "app"},
                                                     binary_root / "mixed_mode_output.txt",
                                                     "mixed direct-source and project-mode check");
    if (!outcome.exited || outcome.exit_code == 0) {
        Fail("mixed direct-source and project-only options should fail");
    }
    ExpectOutputContains(output,
                         "cannot mix a direct source path with project-only options; choose direct-source mode or --project mode",
                         "mixed invocation should fail with a clear mode diagnostic");
}

void TestProjectTestRejectsDirectSourceInvocation(const std::filesystem::path& binary_root,
                                                  const std::filesystem::path& mc_path) {
    const std::filesystem::path source_path = binary_root / "direct_source_test_reject.mc";
    WriteFile(source_path,
              "func main() i32 {\n"
              "    return 0\n"
              "}\n");

    const auto [outcome, output] = RunCommandCapture({mc_path.generic_string(),
                                                      "test",
                                                      source_path.generic_string()},
                                                     binary_root / "direct_source_test_reject_output.txt",
                                                     "direct-source test rejection");
    if (!outcome.exited || outcome.exit_code == 0) {
        Fail("mc test should reject direct-source invocation");
    }
    ExpectOutputContains(output,
                         "mc test does not accept a direct source path; use --project <build.toml>",
                         "mc test should explain that it is project-only");
}

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

    const auto [outcome, output] = RunCommandCapture({mc_path.generic_string(),
                                                      "test",
                                                      "--project",
                                                      (project_root / "build.toml").generic_string(),
                                                      "--target",
                                                      "app"},
                                                     binary_root / "disabled_test_target_output.txt",
                                                     "disabled test target selection");
    if (!outcome.exited || outcome.exit_code == 0) {
        Fail("mc test should fail when the selected target has tests disabled");
    }
    ExpectOutputContains(output,
                         "tests are not enabled for target 'app'; enabled test targets: unit",
                         "disabled test target diagnostic should list enabled test targets");
}

void TestRunExitCodeAndArgs(const std::filesystem::path& binary_root,
                            const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "run_project";
    const std::filesystem::path project_path = WriteBasicProject(
        project_root,
        "func answer() i32 {\n"
        "    return 1\n"
        "}\n",
        "func main(args: Slice<cstr>) i32 {\n"
        "    if args.len == 3 {\n"
        "        return 9\n"
        "    }\n"
        "    return 4\n"
        "}\n");
    const std::filesystem::path build_dir = binary_root / "run_build";
    std::filesystem::remove_all(build_dir);
    const auto [outcome, output] = RunCommandCapture({mc_path.generic_string(),
                                                      "run",
                                                      "--project",
                                                      project_path.generic_string(),
                                                      "--build-dir",
                                                      build_dir.generic_string(),
                                                      "--",
                                                      "alpha",
                                                      "beta"},
                                                     build_dir / "run_output.txt",
                                                     "run command");
    if (!outcome.exited || outcome.exit_code != 9) {
        Fail("mc run should return the executable exit code 9, got output:\n" + output);
    }
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

    const auto [outcome, output] = RunCommandCapture({mc_path.generic_string(),
                                                      "test",
                                                      "--project",
                                                      (project_root / "build.toml").generic_string(),
                                                      "--build-dir",
                                                      build_dir.generic_string()},
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


}  // namespace

namespace mc::tool_tests {

void RunWorkflowToolSuite(const std::filesystem::path& source_root,
                          const std::filesystem::path& binary_root,
                          const std::filesystem::path& mc_path) {
    const std::filesystem::path suite_root = binary_root / "tool" / "workflow";

    TestHelpMentionsRun(suite_root, mc_path);
    TestProjectTestCommandSucceeds(suite_root, mc_path);
    TestProjectTestCommandFailsOnOrdinaryFailure(suite_root, mc_path);
    TestMixedDirectSourceAndProjectOptionsFail(suite_root, mc_path);
    TestProjectTestRejectsDirectSourceInvocation(suite_root, mc_path);
    TestUnknownTargetListsAvailableTargets(suite_root, mc_path);
    TestDisabledTestTargetListsEnabledTargets(suite_root, mc_path);
    TestRunExitCodeAndArgs(suite_root, mc_path);
    TestMissingDefaultTargetFails(suite_root, mc_path);
    TestProjectMissingImportRootFails(source_root, suite_root, mc_path);
    TestProjectAmbiguousImportFails(source_root, suite_root, mc_path);
    TestDuplicateModuleRootFailsEarly(suite_root, mc_path);
    TestProjectTestTimeoutFailsDeterministically(suite_root, mc_path);
    TestDuplicateTargetRootsFailEarly(suite_root, mc_path);
    TestExecutableTargetRejectsNonStaticLibraryLink(suite_root, mc_path);
}

}  // namespace mc::tool_tests
