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

}  // namespace

namespace mc::tool_tests {

void RunWorkflowHelpSuite(const std::filesystem::path& binary_root,
                          const std::filesystem::path& mc_path) {
    TestHelpMentionsRun(binary_root, mc_path);
    TestMixedDirectSourceAndProjectOptionsFail(binary_root, mc_path);
    TestProjectTestRejectsDirectSourceInvocation(binary_root, mc_path);
    TestRunExitCodeAndArgs(binary_root, mc_path);
}

}  // namespace mc::tool_tests
