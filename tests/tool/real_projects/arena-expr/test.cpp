#include <filesystem>
#include <string>

#include "tests/support/process_utils.h"
#include "tests/tool/tool_real_project_suite_internal.h"
#include "tests/tool/tool_suite_common.h"

namespace {

using mc::test_support::ExpectOutputContains;
using mc::tool_tests::RunProjectAndExpectSuccess;
using mc::tool_tests::RunProjectTestAndExpectSuccess;

}  // namespace

namespace mc::tool_tests {

void TestRealArenaExprToolProject(const std::filesystem::path& source_root,
                                  const std::filesystem::path& binary_root,
                                  const std::filesystem::path& mc_path) {
    const std::filesystem::path project_path = source_root / "examples/real/arena_expr/build.toml";
    const std::filesystem::path sample_path = source_root / "examples/real/arena_expr/tests/sample.expr";
    const std::filesystem::path build_dir = binary_root / "arena_expr_build";
    std::filesystem::remove_all(build_dir);

    const std::string expected_output = "((1+2)+(3+4))\n(7+(8+9))\n";

    const std::string run_output = RunProjectAndExpectSuccess(mc_path,
                                                              project_path,
                                                              build_dir,
                                                              {sample_path.generic_string()},
                                                              "arena_expr_run_output.txt",
                                                              "arena expr run");
    ExpectOutputContains(run_output,
                         expected_output,
                         "phase8 arena expr should print deterministic normalized output for each scratch pass");

    const std::string test_output = RunProjectTestAndExpectSuccess(mc_path,
                                                                   project_path,
                                                                   build_dir,
                                                                   "arena_expr_test_output.txt",
                                                                   "arena expr test");
    ExpectOutputContains(test_output,
                         "PASS normalize_text_test.test_normalize_text",
                         "phase8 arena expr ordinary tests should include normalization coverage");
    ExpectOutputContains(test_output,
                         "PASS parse_tree_test.test_parse_tree",
                         "phase8 arena expr ordinary tests should include tree-shape coverage");
    ExpectOutputContains(test_output,
                         "PASS arena_reset_test.test_arena_reset_reuses_scratch",
                         "phase263 arena expr ordinary tests should include arena reset coverage");
    ExpectOutputContains(test_output,
                         "4 tests, 4 passed, 0 failed",
                         "phase8 arena expr test summary should be deterministic");
}

}  // namespace mc::tool_tests