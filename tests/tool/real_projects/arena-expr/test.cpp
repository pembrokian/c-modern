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

void TestRealArenaExprToolProject(const std::filesystem::path& source_root,
                                  const std::filesystem::path& binary_root,
                                  const std::filesystem::path& mc_path) {
    const std::filesystem::path project_path = source_root / "examples/real/arena_expr/build.toml";
    const std::filesystem::path sample_path = source_root / "examples/real/arena_expr/tests/sample.expr";
    const std::filesystem::path build_dir = binary_root / "arena_expr_build";
    std::filesystem::remove_all(build_dir);

    const std::string expected_line = "((1+2)+(3+4))\n";

    const auto [run_outcome, run_output] = RunCommandCapture({mc_path.generic_string(),
                                                              "run",
                                                              "--project",
                                                              project_path.generic_string(),
                                                              "--build-dir",
                                                              build_dir.generic_string(),
                                                              "--",
                                                              sample_path.generic_string()},
                                                             build_dir / "arena_expr_run_output.txt",
                                                             "arena expr run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("phase8 arena expr run should succeed:\n" + run_output);
    }
    ExpectOutputContains(run_output,
                         expected_line,
                         "phase8 arena expr should print the deterministic normalized form");

    const auto [test_outcome, test_output] = RunCommandCapture({mc_path.generic_string(),
                                                                "test",
                                                                "--project",
                                                                project_path.generic_string(),
                                                                "--build-dir",
                                                                build_dir.generic_string()},
                                                               build_dir / "arena_expr_test_output.txt",
                                                               "arena expr test");
    if (!test_outcome.exited || test_outcome.exit_code != 0) {
        Fail("phase8 arena expr tests should pass:\n" + test_output);
    }
    ExpectOutputContains(test_output,
                         "PASS normalize_text_test.test_normalize_text",
                         "phase8 arena expr ordinary tests should include normalization coverage");
    ExpectOutputContains(test_output,
                         "PASS parse_tree_test.test_parse_tree",
                         "phase8 arena expr ordinary tests should include tree-shape coverage");
    ExpectOutputContains(test_output,
                         "3 tests, 3 passed, 0 failed",
                         "phase8 arena expr test summary should be deterministic");
}

}  // namespace mc::tool_tests