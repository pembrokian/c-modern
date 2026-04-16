#include <filesystem>
#include <string>

#include "tests/support/process_utils.h"
#include "tests/tool/tool_real_project_suite_internal.h"

namespace {

using mc::test_support::ExpectOutputContains;
using mc::test_support::Fail;
using mc::test_support::RunCommandCapture;

}  // namespace

namespace mc::tool_tests {

void TestRealGrepLiteProject(const std::filesystem::path& source_root,
                             const std::filesystem::path& binary_root,
                             const std::filesystem::path& mc_path) {
    const std::filesystem::path project_path = source_root / "examples/real/grep_lite/build.toml";
    const std::filesystem::path sample_path = source_root / "examples/real/grep_lite/tests/sample.txt";
    const std::filesystem::path build_dir = binary_root / "grep_lite_build";
    std::filesystem::remove_all(build_dir);

    const auto [run_outcome, run_output] = RunCommandCapture({mc_path.generic_string(),
                                                              "run",
                                                              "--project",
                                                              project_path.generic_string(),
                                                              "--build-dir",
                                                              build_dir.generic_string(),
                                                              "--",
                                                              "alpha",
                                                              sample_path.generic_string()},
                                                             build_dir / "grep_lite_run_output.txt",
                                                             "grep-lite run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("phase8 grep-lite run should succeed:\n" + run_output);
    }
    ExpectOutputContains(run_output,
                         "alpha\nalphabet\n",
                         "phase8 grep-lite run should print matching lines only");
    if (run_output.find("beta\n") != std::string::npos || run_output.find("omega\n") != std::string::npos) {
        Fail("phase8 grep-lite run should not print non-matching lines, got:\n" + run_output);
    }

    const auto [test_outcome, test_output] = RunCommandCapture({mc_path.generic_string(),
                                                                "test",
                                                                "--project",
                                                                project_path.generic_string(),
                                                                "--build-dir",
                                                                build_dir.generic_string()},
                                                               build_dir / "grep_lite_test_output.txt",
                                                               "grep-lite test");
    if (!test_outcome.exited || test_outcome.exit_code != 0) {
        Fail("phase8 grep-lite tests should pass:\n" + test_output);
    }
    ExpectOutputContains(test_output,
                         "PASS contains_match_test.test_contains_match",
                         "phase8 grep-lite ordinary tests should include contains-match coverage");
    ExpectOutputContains(test_output,
                         "PASS count_matches_test.test_count_matches",
                         "phase8 grep-lite ordinary tests should include line-count coverage");
    ExpectOutputContains(test_output,
                         "3 tests, 3 passed, 0 failed",
                         "phase8 grep-lite test summary should be deterministic");
}

}  // namespace mc::tool_tests