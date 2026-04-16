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

void TestRealFileWalkerProject(const std::filesystem::path& source_root,
                               const std::filesystem::path& binary_root,
                               const std::filesystem::path& mc_path) {
    const std::filesystem::path project_path = source_root / "examples/real/file_walker/build.toml";
    const std::filesystem::path sample_root = source_root / "examples/real/file_walker/tests/sample_tree";
    const std::filesystem::path build_dir = binary_root / "file_walker_build";
    std::filesystem::remove_all(build_dir);

    const std::string alpha_path = (sample_root / "alpha.txt").generic_string() + "\n";
    const std::string beta_path = (sample_root / "nested/beta.txt").generic_string() + "\n";
    const std::string gamma_path = (sample_root / "nested/deeper/gamma.txt").generic_string() + "\n";
    const std::string directory_line = (sample_root / "nested").generic_string() + "\n";

    const auto [run_outcome, run_output] = RunCommandCapture({mc_path.generic_string(),
                                                              "run",
                                                              "--project",
                                                              project_path.generic_string(),
                                                              "--build-dir",
                                                              build_dir.generic_string(),
                                                              "--",
                                                              sample_root.generic_string()},
                                                             build_dir / "file_walker_run_output.txt",
                                                             "file walker run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("phase8 file walker run should succeed:\n" + run_output);
    }
    ExpectOutputContains(run_output, alpha_path, "phase8 file walker should print top-level files");
    ExpectOutputContains(run_output, beta_path, "phase8 file walker should recurse into nested directories");
    ExpectOutputContains(run_output, gamma_path, "phase8 file walker should recurse through deeper directories");
    if (run_output.find(directory_line) != std::string::npos) {
        Fail("phase8 file walker should print files only, got:\n" + run_output);
    }

    const auto [test_outcome, test_output] = RunCommandCapture({mc_path.generic_string(),
                                                                "test",
                                                                "--project",
                                                                project_path.generic_string(),
                                                                "--build-dir",
                                                                build_dir.generic_string()},
                                                               build_dir / "file_walker_test_output.txt",
                                                               "file walker test");
    if (!test_outcome.exited || test_outcome.exit_code != 0) {
        Fail("phase8 file walker tests should pass:\n" + test_output);
    }
    ExpectOutputContains(test_output,
                         "PASS entry_name_test.test_entry_name",
                         "phase8 file walker ordinary tests should include entry-name coverage");
    ExpectOutputContains(test_output,
                         "PASS path_join_test.test_path_join",
                         "phase8 file walker ordinary tests should include path-join coverage");
    ExpectOutputContains(test_output,
                         "3 tests, 3 passed, 0 failed",
                         "phase8 file walker test summary should be deterministic");
}

void TestRealHashToolProject(const std::filesystem::path& source_root,
                             const std::filesystem::path& binary_root,
                             const std::filesystem::path& mc_path) {
    const std::filesystem::path project_path = source_root / "examples/real/hash_tool/build.toml";
    const std::filesystem::path sample_path = source_root / "examples/real/hash_tool/tests/sample.txt";
    const std::filesystem::path build_dir = binary_root / "hash_tool_build";
    std::filesystem::remove_all(build_dir);

    const std::string expected_line =
        "d17af4fb11e13fbf  " + sample_path.generic_string() + "\n";

    const auto [run_outcome, run_output] = RunCommandCapture({mc_path.generic_string(),
                                                              "run",
                                                              "--project",
                                                              project_path.generic_string(),
                                                              "--build-dir",
                                                              build_dir.generic_string(),
                                                              "--",
                                                              sample_path.generic_string()},
                                                             build_dir / "hash_tool_run_output.txt",
                                                             "hash tool run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("phase8 hash tool run should succeed:\n" + run_output);
    }
    ExpectOutputContains(run_output,
                         expected_line,
                         "phase8 hash tool should print the deterministic hash line");

    const auto [test_outcome, test_output] = RunCommandCapture({mc_path.generic_string(),
                                                                "test",
                                                                "--project",
                                                                project_path.generic_string(),
                                                                "--build-dir",
                                                                build_dir.generic_string()},
                                                               build_dir / "hash_tool_test_output.txt",
                                                               "hash tool test");
    if (!test_outcome.exited || test_outcome.exit_code != 0) {
        Fail("phase8 hash tool tests should pass:\n" + test_output);
    }
    ExpectOutputContains(test_output,
                         "PASS hash_bytes_test.test_hash_bytes",
                         "phase8 hash tool ordinary tests should include byte-hash coverage");
    ExpectOutputContains(test_output,
                         "PASS hex_u64_test.test_hex_u64",
                         "phase8 hash tool ordinary tests should include hex encoding coverage");
    ExpectOutputContains(test_output,
                         "3 tests, 3 passed, 0 failed",
                         "phase8 hash tool test summary should be deterministic");
}

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