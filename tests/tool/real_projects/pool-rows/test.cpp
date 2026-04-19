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

void TestRealPoolRowsProject(const std::filesystem::path& source_root,
                             const std::filesystem::path& binary_root,
                             const std::filesystem::path& mc_path) {
    const std::filesystem::path project_path = source_root / "examples/real/pool_rows/build.toml";
    const std::filesystem::path build_dir = binary_root / "pool_rows_build";
    std::filesystem::remove_all(build_dir);

    const auto [run_outcome, run_output] = RunCommandCapture({mc_path.generic_string(),
                                                              "run",
                                                              "--project",
                                                              project_path.generic_string(),
                                                              "--build-dir",
                                                              build_dir.generic_string()},
                                                             build_dir / "pool_rows_run_output.txt",
                                                             "pool rows run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("phase262 pool rows run should succeed:\n" + run_output);
    }
    ExpectOutputContains(run_output,
                         "row-0\nrow-1\nrow-2\nrow-3\nrow-4\n",
                         "phase262 pool rows should print the deterministic row batch");

    const auto [test_outcome, test_output] = RunCommandCapture({mc_path.generic_string(),
                                                                "test",
                                                                "--project",
                                                                project_path.generic_string(),
                                                                "--build-dir",
                                                                build_dir.generic_string()},
                                                               build_dir / "pool_rows_test_output.txt",
                                                               "pool rows test");
    if (!test_outcome.exited || test_outcome.exit_code != 0) {
        Fail("phase262 pool rows tests should pass:\n" + test_output);
    }
    ExpectOutputContains(test_output,
                         "PASS format_row_text_test.test_format_row_text",
                         "phase262 pool rows tests should cover row formatting");
    ExpectOutputContains(test_output,
                         "PASS take_return_reuse_test.test_take_return_reuse",
                         "phase262 pool rows tests should cover pool reuse");
    ExpectOutputContains(test_output,
                         "2 tests, 2 passed, 0 failed",
                         "phase262 pool rows test summary should be deterministic");
}

}  // namespace mc::tool_tests