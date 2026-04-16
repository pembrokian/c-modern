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

}  // namespace mc::tool_tests