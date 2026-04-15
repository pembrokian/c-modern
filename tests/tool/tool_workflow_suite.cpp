#include <filesystem>
#include <string>

#include "tests/support/process_utils.h"
#include "tests/tool/tool_suite_common.h"

namespace {

using mc::test_support::ExpectOutputContains;
using mc::test_support::Fail;
using mc::test_support::RunCommandCapture;
using mc::test_support::CopyDirectoryTree;
using mc::test_support::ReadFile;
using mc::test_support::WriteFile;
using namespace mc::tool_tests;
using mc::tool_tests::BuildProjectTargetAndExpectFailure;
using mc::tool_tests::WriteTestProject;

std::filesystem::path InstallKernelResetLaneFixtureProject(const std::filesystem::path& source_root,
                                                           const std::filesystem::path& fixture_root,
                                                           const std::filesystem::path& project_root) {
    constexpr std::string_view kKernelNewSourcePlaceholder = "__KERNEL_NEW_SRC__";

    std::filesystem::remove_all(project_root);
    CopyDirectoryTree(fixture_root, project_root);

    const std::filesystem::path build_toml_path = project_root / "build.toml";
    std::string build_toml_text = ReadFile(build_toml_path);
    if (build_toml_text.find(kKernelNewSourcePlaceholder) == std::string::npos) {
        Fail("kernel reset-lane fixture build.toml is missing the kernel_new source placeholder");
    }

    const std::string kernel_src = (source_root / "kernel" / "src").generic_string();
    size_t pos = 0;
    while ((pos = build_toml_text.find(kKernelNewSourcePlaceholder, pos)) != std::string::npos) {
        build_toml_text.replace(pos, kKernelNewSourcePlaceholder.size(), kernel_src);
        pos += kernel_src.size();
    }
    WriteFile(build_toml_path, build_toml_text);
    return build_toml_path;
}

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
              "func test_failure() *i32 {\n"
              "    return (*i32)((uintptr)(1))\n"
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
    const auto [outcome, output] = RunCommandCapture({mc_path.generic_string(),
                                                      "test",
                                                      "--project",
                                                      project_path.generic_string(),
                                                      "--build-dir",
                                                      build_dir.generic_string()},
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
    const auto [outcome, output] = RunCommandCapture({mc_path.generic_string(),
                                                      "test",
                                                      "--project",
                                                      project_path.generic_string(),
                                                      "--build-dir",
                                                      build_dir.generic_string()},
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

void TestProjectModeMultiFileModulesBuildAndRun(const std::filesystem::path& binary_root,
                                                const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "multifile_project";
    std::filesystem::remove_all(project_root);
    WriteFile(project_root / "build.toml",
              "schema = 1\n"
              "project = \"phase127-multifile-success\"\n"
              "default = \"app\"\n"
              "\n"
              "[targets.app]\n"
              "kind = \"exe\"\n"
              "package = \"app\"\n"
              "root = \"src/main.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"hosted\"\n"
              "\n"
              "[targets.app.search_paths]\n"
              "modules = [\"src\"]\n"
              "\n"
              "[targets.app.module_sets.main]\n"
              "files = [\n"
              "  \"src/main.mc\",\n"
              "  \"src/main_logic.mc\"\n"
              "]\n"
              "\n"
              "[targets.app.module_sets.helper]\n"
              "files = [\n"
              "  \"src/helper_public.mc\",\n"
              "  \"src/helper_private.mc\",\n"
              "  \"src/helper_extra.mc\"\n"
              "]\n"
              "\n"
              "[targets.app.runtime]\n"
              "startup = \"default\"\n");
    WriteFile(project_root / "src/main.mc",
              "import helper\n"
              "\n"
              "func main() i32 {\n"
              "    return helper.answer() + module_bonus()\n"
              "}\n");
    WriteFile(project_root / "src/main_logic.mc",
              "@private\n"
              "func hidden_bonus() i32 {\n"
              "    return 1\n"
              "}\n"
              "\n"
              "func module_bonus() i32 {\n"
              "    return hidden_bonus() + 1\n"
              "}\n");
    WriteFile(project_root / "src/helper_private.mc",
              "@private\n"
              "func hidden_value() i32 {\n"
              "    return 3\n"
              "}\n");
    WriteFile(project_root / "src/helper_extra.mc",
              "func extra_value() i32 {\n"
              "    return 5\n"
              "}\n");
    WriteFile(project_root / "src/helper_public.mc",
              "func answer() i32 {\n"
              "    return hidden_value() + extra_value()\n"
              "}\n");

    const std::filesystem::path build_dir = binary_root / "multifile_build";
    std::filesystem::remove_all(build_dir);
    BuildProjectTargetAndExpectSuccess(mc_path,
                                       project_root / "build.toml",
                                       build_dir,
                                       "app",
                                       "multifile_build_output.txt",
                                       "multi-file module build");

    const auto [run_outcome, run_output] = RunCommandCapture({mc_path.generic_string(),
                                                              "run",
                                                              "--project",
                                                              (project_root / "build.toml").generic_string(),
                                                              "--build-dir",
                                                              build_dir.generic_string()},
                                                             build_dir / "multifile_run_output.txt",
                                                             "multi-file module run");
    if (!run_outcome.exited || run_outcome.exit_code != 10) {
        Fail("multi-file module project should return 10, got:\n" + run_output);
    }
}


void TestDuplicateModuleSetFileOwnershipFails(const std::filesystem::path& binary_root,
                                              const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "multifile_duplicate_ownership_project";
    std::filesystem::remove_all(project_root);
    WriteFile(project_root / "build.toml",
              "schema = 1\n"
              "project = \"phase127-duplicate-ownership\"\n"
              "default = \"app\"\n"
              "\n"
              "[targets.app]\n"
              "kind = \"exe\"\n"
              "package = \"app\"\n"
              "root = \"src/main.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"hosted\"\n"
              "\n"
              "[targets.app.search_paths]\n"
              "modules = [\"src\"]\n"
              "\n"
              "[targets.app.module_sets.main]\n"
              "files = [\"src/main.mc\", \"src/shared.mc\"]\n"
              "\n"
              "[targets.app.module_sets.helper]\n"
              "files = [\"src/helper.mc\", \"src/shared.mc\"]\n"
              "\n"
              "[targets.app.runtime]\n"
              "startup = \"default\"\n");
    WriteFile(project_root / "src/main.mc", "func main() i32 { return 0 }\n");
    WriteFile(project_root / "src/shared.mc", "func shared() i32 { return 1 }\n");
    WriteFile(project_root / "src/helper.mc", "func answer() i32 { return 2 }\n");

    const std::filesystem::path build_dir = binary_root / "multifile_duplicate_ownership_build";
    std::filesystem::remove_all(build_dir);
    const std::string output = BuildProjectTargetAndExpectFailure(mc_path,
                                                                  project_root / "build.toml",
                                                                  build_dir,
                                                                  "app",
                                                                  "multifile_duplicate_ownership_output.txt",
                                                                  "duplicate module-set ownership build");
    ExpectOutputContains(output,
                         "assigns source file",
                         "duplicate module-set file ownership should be rejected");
    ExpectOutputContains(output,
                         "multiple module sets",
                         "duplicate module-set file ownership diagnostic should explain the conflict");
}

void TestDuplicateTopLevelAcrossModulePartsFails(const std::filesystem::path& binary_root,
                                                 const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "multifile_duplicate_symbol_project";
    std::filesystem::remove_all(project_root);
    WriteFile(project_root / "build.toml",
              "schema = 1\n"
              "project = \"phase127-duplicate-symbol\"\n"
              "default = \"app\"\n"
              "\n"
              "[targets.app]\n"
              "kind = \"exe\"\n"
              "package = \"app\"\n"
              "root = \"src/main.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"hosted\"\n"
              "\n"
              "[targets.app.search_paths]\n"
              "modules = [\"src\"]\n"
              "\n"
              "[targets.app.module_sets.helper]\n"
              "files = [\"src/helper_a.mc\", \"src/helper_b.mc\"]\n"
              "\n"
              "[targets.app.runtime]\n"
              "startup = \"default\"\n");
    WriteFile(project_root / "src/main.mc",
              "import helper\n"
              "\n"
              "func main() i32 {\n"
              "    return helper.answer()\n"
              "}\n");
    WriteFile(project_root / "src/helper_a.mc",
              "func answer() i32 {\n"
              "    return 1\n"
              "}\n");
    WriteFile(project_root / "src/helper_b.mc",
              "func answer() i32 {\n"
              "    return 2\n"
              "}\n");

    const std::filesystem::path build_dir = binary_root / "multifile_duplicate_symbol_build";
    std::filesystem::remove_all(build_dir);
    const std::string output = BuildProjectTargetAndExpectFailure(mc_path,
                                                                  project_root / "build.toml",
                                                                  build_dir,
                                                                  "app",
                                                                  "multifile_duplicate_symbol_output.txt",
                                                                  "duplicate symbol across module parts build");
    ExpectOutputContains(output,
                         "duplicate top-level value symbol: answer",
                         "duplicate top-level declarations across module parts should fail deterministically");
}

void TestKernelResetLaneRepoProjectRuns(const std::filesystem::path& source_root,
                                        const std::filesystem::path& binary_root,
                                        const std::filesystem::path& mc_path) {
    const std::filesystem::path project_path = source_root / "kernel" / "build.toml";
    const std::filesystem::path build_dir = binary_root / "kernel_reset_lane_repo_build";
    std::filesystem::remove_all(build_dir);

    BuildProjectTargetAndExpectSuccess(mc_path,
                                       project_path,
                                       build_dir,
                                       "kernel",
                                       "kernel_reset_lane_repo_build_output.txt",
                                       "kernel reset lane repo project build");

    const auto [run_outcome, run_output] = RunCommandCapture({mc_path.generic_string(),
                                                              "run",
                                                              "--project",
                                                              project_path.generic_string(),
                                                              "--target",
                                                              "kernel",
                                                              "--build-dir",
                                                              build_dir.generic_string()},
                                                             build_dir / "kernel_reset_lane_repo_run_output.txt",
                                                             "kernel reset lane repo project run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("kernel reset lane repo project should return 0, got:\n" + run_output);
    }
}

void TestKernelResetLaneSmokeProjectRuns(const std::filesystem::path& source_root,
                                         const std::filesystem::path& binary_root,
                                         const std::filesystem::path& mc_path) {
    const std::filesystem::path fixture_root = source_root / "tests" / "smoke" / "kernel_reset_lane_serial_round_trip";
    const std::filesystem::path project_root = binary_root / "kernel_reset_lane_smoke_project";
    const std::filesystem::path project_path =
        InstallKernelResetLaneFixtureProject(source_root, fixture_root, project_root);

    const std::filesystem::path build_dir = binary_root / "kernel_reset_lane_smoke_build";
    std::filesystem::remove_all(build_dir);
    BuildProjectTargetAndExpectSuccess(mc_path,
                                       project_path,
                                       build_dir,
                                       "app",
                                       "kernel_reset_lane_smoke_build_output.txt",
                                       "kernel reset lane smoke build");

    const auto [run_outcome, run_output] = RunCommandCapture({mc_path.generic_string(),
                                                              "run",
                                                              "--project",
                                                              project_path.generic_string(),
                                                              "--build-dir",
                                                              build_dir.generic_string()},
                                                             build_dir / "kernel_reset_lane_smoke_run_output.txt",
                                                             "kernel reset lane smoke run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("kernel reset lane smoke project should return 0, got:\n" + run_output);
    }
}

void TestKernelResetLaneRetainedStateProjectRuns(const std::filesystem::path& source_root,
                                                 const std::filesystem::path& binary_root,
                                                 const std::filesystem::path& mc_path) {
    const std::filesystem::path fixture_root = source_root / "tests" / "system" / "kernel_reset_lane_retained_log";
    const std::filesystem::path project_root = binary_root / "kernel_reset_lane_retained_state_project";
    const std::filesystem::path project_path =
        InstallKernelResetLaneFixtureProject(source_root, fixture_root, project_root);

    const std::filesystem::path build_dir = binary_root / "kernel_reset_lane_retained_state_build";
    std::filesystem::remove_all(build_dir);
    BuildProjectTargetAndExpectSuccess(mc_path,
                                       project_path,
                                       build_dir,
                                       "app",
                                       "kernel_reset_lane_retained_state_build_output.txt",
                                       "kernel reset lane retained-state build");

    const auto [run_outcome, run_output] = RunCommandCapture({mc_path.generic_string(),
                                                              "run",
                                                              "--project",
                                                              project_path.generic_string(),
                                                              "--build-dir",
                                                              build_dir.generic_string()},
                                                             build_dir / "kernel_reset_lane_retained_state_run_output.txt",
                                                             "kernel reset lane retained-state run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("kernel reset lane retained-state project should return 0, got:\n" + run_output);
    }
}

void TestKernelResetLaneObservabilityProjectRuns(const std::filesystem::path& source_root,
                                                 const std::filesystem::path& binary_root,
                                                 const std::filesystem::path& mc_path) {
    const std::filesystem::path fixture_root = source_root / "tests" / "system" / "kernel_reset_lane_serial_observability";
    const std::filesystem::path project_root = binary_root / "kernel_reset_lane_observability_project";
    const std::filesystem::path project_path =
        InstallKernelResetLaneFixtureProject(source_root, fixture_root, project_root);

    const std::filesystem::path build_dir = binary_root / "kernel_reset_lane_observability_build";
    std::filesystem::remove_all(build_dir);
    BuildProjectTargetAndExpectSuccess(mc_path,
                                       project_path,
                                       build_dir,
                                       "app",
                                       "kernel_reset_lane_observability_build_output.txt",
                                       "kernel reset lane observability build");

    const auto [run_outcome, run_output] = RunCommandCapture({mc_path.generic_string(),
                                                              "run",
                                                              "--project",
                                                              project_path.generic_string(),
                                                              "--build-dir",
                                                              build_dir.generic_string()},
                                                             build_dir / "kernel_reset_lane_observability_run_output.txt",
                                                             "kernel reset lane observability run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("kernel reset lane observability project should return 0, got:\n" + run_output);
    }
}

void TestKernelResetLaneKvRoundtripProjectRuns(const std::filesystem::path& source_root,
                                               const std::filesystem::path& binary_root,
                                               const std::filesystem::path& mc_path) {
    const std::filesystem::path fixture_root = source_root / "tests" / "system" / "kernel_reset_lane_kv_roundtrip";
    const std::filesystem::path project_root = binary_root / "kernel_reset_lane_kv_roundtrip_project";
    const std::filesystem::path project_path =
        InstallKernelResetLaneFixtureProject(source_root, fixture_root, project_root);

    const std::filesystem::path build_dir = binary_root / "kernel_reset_lane_kv_roundtrip_build";
    std::filesystem::remove_all(build_dir);
    BuildProjectTargetAndExpectSuccess(mc_path,
                                       project_path,
                                       build_dir,
                                       "app",
                                       "kernel_reset_lane_kv_roundtrip_build_output.txt",
                                       "kernel reset lane kv-roundtrip build");

    const auto [run_outcome, run_output] = RunCommandCapture({mc_path.generic_string(),
                                                              "run",
                                                              "--project",
                                                              project_path.generic_string(),
                                                              "--build-dir",
                                                              build_dir.generic_string()},
                                                             build_dir / "kernel_reset_lane_kv_roundtrip_run_output.txt",
                                                             "kernel reset lane kv-roundtrip run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("kernel reset lane kv-roundtrip project should return 0, got:\n" + run_output);
    }
}

void TestKernelResetLaneServiceCompositionProjectRuns(const std::filesystem::path& source_root,
                                                      const std::filesystem::path& binary_root,
                                                      const std::filesystem::path& mc_path) {
    const std::filesystem::path fixture_root = source_root / "tests" / "system" / "kernel_reset_lane_service_composition";
    const std::filesystem::path project_root = binary_root / "kernel_reset_lane_service_composition_project";
    const std::filesystem::path project_path =
        InstallKernelResetLaneFixtureProject(source_root, fixture_root, project_root);

    const std::filesystem::path build_dir = binary_root / "kernel_reset_lane_service_composition_build";
    std::filesystem::remove_all(build_dir);
    BuildProjectTargetAndExpectSuccess(mc_path,
                                       project_path,
                                       build_dir,
                                       "app",
                                       "kernel_reset_lane_service_composition_build_output.txt",
                                       "kernel reset lane service composition build");

    const auto [run_outcome, run_output] = RunCommandCapture({mc_path.generic_string(),
                                                              "run",
                                                              "--project",
                                                              project_path.generic_string(),
                                                              "--build-dir",
                                                              build_dir.generic_string()},
                                                             build_dir / "kernel_reset_lane_service_composition_run_output.txt",
                                                             "kernel reset lane service composition run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("kernel reset lane service composition project should return 0, got:\n" + run_output);
    }
}

void TestKernelResetLaneBootProjectRuns(const std::filesystem::path& source_root,
                                        const std::filesystem::path& binary_root,
                                        const std::filesystem::path& mc_path) {
    const std::filesystem::path fixture_root = source_root / "tests" / "smoke" / "kernel_reset_lane_boot";
    const std::filesystem::path project_root = binary_root / "kernel_reset_lane_boot_project";
    const std::filesystem::path project_path =
        InstallKernelResetLaneFixtureProject(source_root, fixture_root, project_root);

    const std::filesystem::path build_dir = binary_root / "kernel_reset_lane_boot_build";
    std::filesystem::remove_all(build_dir);
    BuildProjectTargetAndExpectSuccess(mc_path,
                                       project_path,
                                       build_dir,
                                       "app",
                                       "kernel_reset_lane_boot_build_output.txt",
                                       "kernel reset lane boot build");

    const auto [run_outcome, run_output] = RunCommandCapture({mc_path.generic_string(),
                                                              "run",
                                                              "--project",
                                                              project_path.generic_string(),
                                                              "--build-dir",
                                                              build_dir.generic_string()},
                                                             build_dir / "kernel_reset_lane_boot_run_output.txt",
                                                             "kernel reset lane boot run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("kernel reset lane boot project should return 0, got:\n" + run_output);
    }
}

void TestKernelResetLaneImageProjectRuns(const std::filesystem::path& source_root,
                                         const std::filesystem::path& binary_root,
                                         const std::filesystem::path& mc_path) {
    const std::filesystem::path fixture_root = source_root / "tests" / "smoke" / "kernel_reset_lane_image";
    const std::filesystem::path project_root = binary_root / "kernel_reset_lane_image_project";
    const std::filesystem::path project_path =
        InstallKernelResetLaneFixtureProject(source_root, fixture_root, project_root);

    const std::filesystem::path build_dir = binary_root / "kernel_reset_lane_image_build";
    std::filesystem::remove_all(build_dir);
    BuildProjectTargetAndExpectSuccess(mc_path,
                                       project_path,
                                       build_dir,
                                       "app",
                                       "kernel_reset_lane_image_build_output.txt",
                                       "kernel reset lane image build");

    const auto [run_outcome, run_output] = RunCommandCapture({mc_path.generic_string(),
                                                              "run",
                                                              "--project",
                                                              project_path.generic_string(),
                                                              "--build-dir",
                                                              build_dir.generic_string()},
                                                             build_dir / "kernel_reset_lane_image_run_output.txt",
                                                             "kernel reset lane image run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("kernel reset lane image project should return 0, got:\n" + run_output);
    }
}

void TestKernelResetLaneServiceIdentityProjectRuns(const std::filesystem::path& source_root,
                                                   const std::filesystem::path& binary_root,
                                                   const std::filesystem::path& mc_path) {
    const std::filesystem::path fixture_root = source_root / "tests" / "system" / "kernel_reset_lane_service_identity";
    const std::filesystem::path project_root = binary_root / "kernel_reset_lane_service_identity_project";
    const std::filesystem::path project_path =
        InstallKernelResetLaneFixtureProject(source_root, fixture_root, project_root);

    const std::filesystem::path build_dir = binary_root / "kernel_reset_lane_service_identity_build";
    std::filesystem::remove_all(build_dir);
    BuildProjectTargetAndExpectSuccess(mc_path,
                                       project_path,
                                       build_dir,
                                       "app",
                                       "kernel_reset_lane_service_identity_build_output.txt",
                                       "kernel reset lane service identity build");

    const auto [run_outcome, run_output] = RunCommandCapture({mc_path.generic_string(),
                                                              "run",
                                                              "--project",
                                                              project_path.generic_string(),
                                                              "--build-dir",
                                                              build_dir.generic_string()},
                                                             build_dir / "kernel_reset_lane_service_identity_run_output.txt",
                                                             "kernel reset lane service identity run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("kernel reset lane service identity project should return 0, got:\n" + run_output);
    }
}

void TestKernelResetLaneTemporalBackpressureProjectRuns(const std::filesystem::path& source_root,
                                                        const std::filesystem::path& binary_root,
                                                        const std::filesystem::path& mc_path) {
    const std::filesystem::path fixture_root = source_root / "tests" / "system" / "kernel_reset_lane_temporal_backpressure";
    const std::filesystem::path project_root = binary_root / "kernel_reset_lane_temporal_backpressure_project";
    const std::filesystem::path project_path =
        InstallKernelResetLaneFixtureProject(source_root, fixture_root, project_root);

    const std::filesystem::path build_dir = binary_root / "kernel_reset_lane_temporal_backpressure_build";
    std::filesystem::remove_all(build_dir);
    BuildProjectTargetAndExpectSuccess(mc_path,
                                       project_path,
                                       build_dir,
                                       "app",
                                       "kernel_reset_lane_temporal_backpressure_build_output.txt",
                                       "kernel reset lane temporal backpressure build");

    const auto [run_outcome, run_output] = RunCommandCapture({mc_path.generic_string(),
                                                              "run",
                                                              "--project",
                                                              project_path.generic_string(),
                                                              "--build-dir",
                                                              build_dir.generic_string()},
                                                             build_dir / "kernel_reset_lane_temporal_backpressure_run_output.txt",
                                                             "kernel reset lane temporal backpressure run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("kernel reset lane temporal backpressure project should return 0, got:\n" + run_output);
    }
}

void TestKernelResetLaneMultiClientProjectRuns(const std::filesystem::path& source_root,
                                               const std::filesystem::path& binary_root,
                                               const std::filesystem::path& mc_path) {
    const std::filesystem::path fixture_root = source_root / "tests" / "system" / "kernel_reset_lane_multi_client";
    const std::filesystem::path project_root = binary_root / "kernel_reset_lane_multi_client_project";
    const std::filesystem::path project_path =
        InstallKernelResetLaneFixtureProject(source_root, fixture_root, project_root);

    const std::filesystem::path build_dir = binary_root / "kernel_reset_lane_multi_client_build";
    std::filesystem::remove_all(build_dir);
    BuildProjectTargetAndExpectSuccess(mc_path,
                                       project_path,
                                       build_dir,
                                       "app",
                                       "kernel_reset_lane_multi_client_build_output.txt",
                                       "kernel reset lane multi-client build");

    const auto [run_outcome, run_output] = RunCommandCapture({mc_path.generic_string(),
                                                              "run",
                                                              "--project",
                                                              project_path.generic_string(),
                                                              "--build-dir",
                                                              build_dir.generic_string()},
                                                             build_dir / "kernel_reset_lane_multi_client_run_output.txt",
                                                             "kernel reset lane multi-client run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("kernel reset lane multi-client project should return 0, got:\n" + run_output);
    }
}

void TestKernelResetLaneLongLivedCoherenceProjectRuns(const std::filesystem::path& source_root,
                                                      const std::filesystem::path& binary_root,
                                                      const std::filesystem::path& mc_path) {
    const std::filesystem::path fixture_root = source_root / "tests" / "system" / "kernel_reset_lane_long_lived_coherence";
    const std::filesystem::path project_root = binary_root / "kernel_reset_lane_long_lived_coherence_project";
    const std::filesystem::path project_path =
        InstallKernelResetLaneFixtureProject(source_root, fixture_root, project_root);

    const std::filesystem::path build_dir = binary_root / "kernel_reset_lane_long_lived_coherence_build";
    std::filesystem::remove_all(build_dir);
    BuildProjectTargetAndExpectSuccess(mc_path,
                                       project_path,
                                       build_dir,
                                       "app",
                                       "kernel_reset_lane_long_lived_coherence_build_output.txt",
                                       "kernel reset lane long-lived coherence build");

    const auto [run_outcome, run_output] = RunCommandCapture({mc_path.generic_string(),
                                                              "run",
                                                              "--project",
                                                              project_path.generic_string(),
                                                              "--build-dir",
                                                              build_dir.generic_string()},
                                                             build_dir / "kernel_reset_lane_long_lived_coherence_run_output.txt",
                                                             "kernel reset lane long-lived coherence run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("kernel reset lane long-lived coherence project should return 0, got:\n" + run_output);
    }
}

void TestKernelResetLaneCrossServiceFailureProjectRuns(const std::filesystem::path& source_root,
                                                       const std::filesystem::path& binary_root,
                                                       const std::filesystem::path& mc_path) {
    const std::filesystem::path fixture_root = source_root / "tests" / "system" / "kernel_reset_lane_cross_service_failure";
    const std::filesystem::path project_root = binary_root / "kernel_reset_lane_cross_service_failure_project";
    const std::filesystem::path project_path =
        InstallKernelResetLaneFixtureProject(source_root, fixture_root, project_root);

    const std::filesystem::path build_dir = binary_root / "kernel_reset_lane_cross_service_failure_build";
    std::filesystem::remove_all(build_dir);
    BuildProjectTargetAndExpectSuccess(mc_path,
                                       project_path,
                                       build_dir,
                                       "app",
                                       "kernel_reset_lane_cross_service_failure_build_output.txt",
                                       "kernel reset lane cross-service failure build");

    const auto [run_outcome, run_output] = RunCommandCapture({mc_path.generic_string(),
                                                              "run",
                                                              "--project",
                                                              project_path.generic_string(),
                                                              "--build-dir",
                                                              build_dir.generic_string()},
                                                             build_dir / "kernel_reset_lane_cross_service_failure_run_output.txt",
                                                             "kernel reset lane cross-service failure run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("kernel reset lane cross-service failure project should return 0, got:\n" + run_output);
    }
}

void TestKernelResetLanePhase159ObservabilityProjectRuns(const std::filesystem::path& source_root,
                                                         const std::filesystem::path& binary_root,
                                                         const std::filesystem::path& mc_path) {
    const std::filesystem::path fixture_root = source_root / "tests" / "system" / "kernel_reset_lane_observability";
    const std::filesystem::path project_root = binary_root / "kernel_reset_lane_phase159_observability_project";
    const std::filesystem::path project_path =
        InstallKernelResetLaneFixtureProject(source_root, fixture_root, project_root);

    const std::filesystem::path build_dir = binary_root / "kernel_reset_lane_phase159_observability_build";
    std::filesystem::remove_all(build_dir);
    BuildProjectTargetAndExpectSuccess(mc_path,
                                       project_path,
                                       build_dir,
                                       "app",
                                       "kernel_reset_lane_phase159_observability_build_output.txt",
                                       "kernel reset lane phase 159 observability build");

    const auto [run_outcome, run_output] = RunCommandCapture({mc_path.generic_string(),
                                                              "run",
                                                              "--project",
                                                              project_path.generic_string(),
                                                              "--build-dir",
                                                              build_dir.generic_string()},
                                                             build_dir / "kernel_reset_lane_phase159_observability_run_output.txt",
                                                             "kernel reset lane phase 159 observability run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("kernel reset lane phase 159 observability project should return 0, got:\n" + run_output);
    }
}

void TestKernelResetLanePhase160ModelBoundaryProjectRuns(const std::filesystem::path& source_root,
                                                         const std::filesystem::path& binary_root,
                                                         const std::filesystem::path& mc_path) {
    const std::filesystem::path fixture_root = source_root / "tests" / "system" / "kernel_reset_lane_model_boundary";
    const std::filesystem::path project_root = binary_root / "kernel_reset_lane_phase160_model_boundary_project";
    const std::filesystem::path project_path =
        InstallKernelResetLaneFixtureProject(source_root, fixture_root, project_root);

    const std::filesystem::path build_dir = binary_root / "kernel_reset_lane_phase160_model_boundary_build";
    std::filesystem::remove_all(build_dir);
    BuildProjectTargetAndExpectSuccess(mc_path,
                                       project_path,
                                       build_dir,
                                       "app",
                                       "kernel_reset_lane_phase160_model_boundary_build_output.txt",
                                       "kernel reset lane phase 160 model boundary build");

    const auto [run_outcome, run_output] = RunCommandCapture({mc_path.generic_string(),
                                                              "run",
                                                              "--project",
                                                              project_path.generic_string(),
                                                              "--build-dir",
                                                              build_dir.generic_string()},
                                                             build_dir / "kernel_reset_lane_phase160_model_boundary_run_output.txt",
                                                             "kernel reset lane phase 160 model boundary run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("kernel reset lane phase 160 model boundary project should return 0, got:\n" + run_output);
    }
}


void TestKernelResetLanePhase161DeliveryWitnessProjectRuns(const std::filesystem::path& source_root,
                                                           const std::filesystem::path& binary_root,
                                                           const std::filesystem::path& mc_path) {
    const std::filesystem::path fixture_root = source_root / "tests" / "system" / "kernel_reset_lane_delivery_witness";
    const std::filesystem::path project_root = binary_root / "kernel_reset_lane_phase161_delivery_witness_project";
    const std::filesystem::path project_path =
        InstallKernelResetLaneFixtureProject(source_root, fixture_root, project_root);

    const std::filesystem::path build_dir = binary_root / "kernel_reset_lane_phase161_delivery_witness_build";
    std::filesystem::remove_all(build_dir);
    BuildProjectTargetAndExpectSuccess(mc_path,
                                       project_path,
                                       build_dir,
                                       "app",
                                       "kernel_reset_lane_phase161_delivery_witness_build_output.txt",
                                       "kernel reset lane phase 161 delivery witness build");

    const auto [run_outcome, run_output] = RunCommandCapture({mc_path.generic_string(),
                                                              "run",
                                                              "--project",
                                                              project_path.generic_string(),
                                                              "--build-dir",
                                                              build_dir.generic_string()},
                                                             build_dir / "kernel_reset_lane_phase161_delivery_witness_run_output.txt",
                                                             "kernel reset lane phase 161 delivery witness run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("kernel reset lane phase 161 delivery witness project should return 0, got:\n" + run_output);
    }
}


void TestKernelResetLanePhase170StaticTopologyProjectRuns(const std::filesystem::path& source_root,
                                                          const std::filesystem::path& binary_root,
                                                          const std::filesystem::path& mc_path) {
    const std::filesystem::path fixture_root = source_root / "tests" / "system" / "kernel_reset_lane_static_topology";
    const std::filesystem::path project_root = binary_root / "kernel_reset_lane_phase170_static_topology_project";
    const std::filesystem::path project_path =
        InstallKernelResetLaneFixtureProject(source_root, fixture_root, project_root);

    const std::filesystem::path build_dir = binary_root / "kernel_reset_lane_phase170_static_topology_build";
    std::filesystem::remove_all(build_dir);
    BuildProjectTargetAndExpectSuccess(mc_path,
                                       project_path,
                                       build_dir,
                                       "app",
                                       "kernel_reset_lane_phase170_static_topology_build_output.txt",
                                       "kernel reset lane phase 170 static topology build");

    const auto [run_outcome, run_output] = RunCommandCapture({mc_path.generic_string(),
                                                              "run",
                                                              "--project",
                                                              project_path.generic_string(),
                                                              "--build-dir",
                                                              build_dir.generic_string()},
                                                             build_dir / "kernel_reset_lane_phase170_static_topology_run_output.txt",
                                                             "kernel reset lane phase 170 static topology run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("kernel reset lane phase 170 static topology project should return 0, got:\n" + run_output);
    }
}

void TestKernelResetLanePhase173SmpBoundaryProjectRuns(const std::filesystem::path& source_root,
                                                       const std::filesystem::path& binary_root,
                                                       const std::filesystem::path& mc_path) {
    const std::filesystem::path fixture_root = source_root / "tests" / "system" / "kernel_reset_lane_smp_boundary";
    const std::filesystem::path project_root = binary_root / "kernel_reset_lane_phase173_smp_boundary_project";
    const std::filesystem::path project_path =
        InstallKernelResetLaneFixtureProject(source_root, fixture_root, project_root);

    const std::filesystem::path build_dir = binary_root / "kernel_reset_lane_phase173_smp_boundary_build";
    std::filesystem::remove_all(build_dir);
    BuildProjectTargetAndExpectSuccess(mc_path,
                                       project_path,
                                       build_dir,
                                       "app",
                                       "kernel_reset_lane_phase173_smp_boundary_build_output.txt",
                                       "kernel reset lane phase 173 smp boundary build");

    const auto [run_outcome, run_output] = RunCommandCapture({mc_path.generic_string(),
                                                              "run",
                                                              "--project",
                                                              project_path.generic_string(),
                                                              "--build-dir",
                                                              build_dir.generic_string()},
                                                             build_dir / "kernel_reset_lane_phase173_smp_boundary_run_output.txt",
                                                             "kernel reset lane phase 173 smp boundary run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("kernel reset lane phase 173 smp boundary project should return 0, got:\n" + run_output);
    }
}

void TestKernelResetLanePhase176GrowthProjectRuns(const std::filesystem::path& source_root,
                                                  const std::filesystem::path& binary_root,
                                                  const std::filesystem::path& mc_path) {
    const std::filesystem::path fixture_root = source_root / "tests" / "system" / "kernel_reset_lane_phase176_growth";
    const std::filesystem::path project_root = binary_root / "kernel_reset_lane_phase176_growth_project";
    const std::filesystem::path project_path =
        InstallKernelResetLaneFixtureProject(source_root, fixture_root, project_root);

    const std::filesystem::path build_dir = binary_root / "kernel_reset_lane_phase176_growth_build";
    std::filesystem::remove_all(build_dir);
    BuildProjectTargetAndExpectSuccess(mc_path,
                                       project_path,
                                       build_dir,
                                       "app",
                                       "kernel_reset_lane_phase176_growth_build_output.txt",
                                       "kernel reset lane phase 176 growth build");

    const auto [run_outcome, run_output] = RunCommandCapture({mc_path.generic_string(),
                                                              "run",
                                                              "--project",
                                                              project_path.generic_string(),
                                                              "--build-dir",
                                                              build_dir.generic_string()},
                                                             build_dir / "kernel_reset_lane_phase176_growth_run_output.txt",
                                                             "kernel reset lane phase 176 growth run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("kernel reset lane phase 176 growth project should return 0, got:\n" + run_output);
    }
}

void TestKernelResetLanePhase177HostileShellProjectRuns(const std::filesystem::path& source_root,
                                                        const std::filesystem::path& binary_root,
                                                        const std::filesystem::path& mc_path) {
    const std::filesystem::path fixture_root = source_root / "tests" / "system" / "kernel_reset_lane_phase177_hostile_shell";
    const std::filesystem::path project_root = binary_root / "kernel_reset_lane_phase177_hostile_shell_project";
    const std::filesystem::path project_path =
        InstallKernelResetLaneFixtureProject(source_root, fixture_root, project_root);

    const std::filesystem::path build_dir = binary_root / "kernel_reset_lane_phase177_hostile_shell_build";
    std::filesystem::remove_all(build_dir);
    BuildProjectTargetAndExpectSuccess(mc_path,
                                       project_path,
                                       build_dir,
                                       "app",
                                       "kernel_reset_lane_phase177_hostile_shell_build_output.txt",
                                       "kernel reset lane phase 177 hostile shell build");

    const auto [run_outcome, run_output] = RunCommandCapture({mc_path.generic_string(),
                                                              "run",
                                                              "--project",
                                                              project_path.generic_string(),
                                                              "--build-dir",
                                                              build_dir.generic_string()},
                                                             build_dir / "kernel_reset_lane_phase177_hostile_shell_run_output.txt",
                                                             "kernel reset lane phase 177 hostile shell run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("kernel reset lane phase 177 hostile shell project should return 0, got:\n" + run_output);
    }
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
    TestDuplicateOrdinaryTestModuleNamesExplainBootstrapRule(suite_root, mc_path);
    TestGeneratedOrdinaryRunnerRequiresRepositoryStdlibRoot(suite_root, mc_path);
    TestDuplicateTargetRootsFailEarly(suite_root, mc_path);
    TestExecutableTargetRejectsNonStaticLibraryLink(suite_root, mc_path);
    TestProjectModeMultiFileModulesBuildAndRun(suite_root, mc_path);
    TestDuplicateModuleSetFileOwnershipFails(suite_root, mc_path);
    TestDuplicateTopLevelAcrossModulePartsFails(suite_root, mc_path);
    TestKernelResetLaneRepoProjectRuns(source_root, suite_root, mc_path);
    TestKernelResetLaneSmokeProjectRuns(source_root, suite_root, mc_path);
    TestKernelResetLaneRetainedStateProjectRuns(source_root, suite_root, mc_path);
    TestKernelResetLaneObservabilityProjectRuns(source_root, suite_root, mc_path);
    TestKernelResetLaneKvRoundtripProjectRuns(source_root, suite_root, mc_path);
    TestKernelResetLaneServiceCompositionProjectRuns(source_root, suite_root, mc_path);
    TestKernelResetLaneBootProjectRuns(source_root, suite_root, mc_path);
    TestKernelResetLaneImageProjectRuns(source_root, suite_root, mc_path);
    TestKernelResetLaneServiceIdentityProjectRuns(source_root, suite_root, mc_path);
    TestKernelResetLaneTemporalBackpressureProjectRuns(source_root, suite_root, mc_path);
    TestKernelResetLaneMultiClientProjectRuns(source_root, suite_root, mc_path);
    TestKernelResetLaneLongLivedCoherenceProjectRuns(source_root, suite_root, mc_path);
    TestKernelResetLaneCrossServiceFailureProjectRuns(source_root, suite_root, mc_path);
    TestKernelResetLanePhase159ObservabilityProjectRuns(source_root, suite_root, mc_path);
    TestKernelResetLanePhase160ModelBoundaryProjectRuns(source_root, suite_root, mc_path);
    TestKernelResetLanePhase161DeliveryWitnessProjectRuns(source_root, suite_root, mc_path);
    TestKernelResetLanePhase170StaticTopologyProjectRuns(source_root, suite_root, mc_path);
    TestKernelResetLanePhase173SmpBoundaryProjectRuns(source_root, suite_root, mc_path);
    TestKernelResetLanePhase176GrowthProjectRuns(source_root, suite_root, mc_path);
    TestKernelResetLanePhase177HostileShellProjectRuns(source_root, suite_root, mc_path);
}

}  // namespace mc::tool_tests
