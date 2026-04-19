#include <filesystem>
#include <numeric>
#include <string>

#include "tests/support/process_utils.h"
#include "tests/tool/tool_real_project_suite_internal.h"

namespace {

using mc::test_support::ExpectOutputContains;
using mc::test_support::Fail;
using mc::test_support::RunCommandCapture;
using mc::test_support::WriteFile;

}  // namespace

namespace mc::tool_tests {

void TestRealBundleStageProject(const std::filesystem::path& source_root,
                                const std::filesystem::path& binary_root,
                                const std::filesystem::path& mc_path) {
    const std::filesystem::path project_path = source_root / "examples/real/bundle_stage/build.toml";
    const std::filesystem::path build_dir = binary_root / "bundle_stage_build";
    std::filesystem::remove_all(build_dir);

    std::string sample(5000, 'A');
    sample.back() = 'B';
    const std::filesystem::path sample_path = build_dir / "bundle.bin";
    WriteFile(sample_path, sample);

    const auto sample_sum = std::accumulate(sample.begin(),
                                            sample.end(),
                                            std::uint64_t{0},
                                            [](std::uint64_t acc, char ch) {
                                                return acc + static_cast<unsigned char>(ch);
                                            });

    const auto [run_outcome, run_output] = RunCommandCapture({mc_path.generic_string(),
                                                              "run",
                                                              "--project",
                                                              project_path.generic_string(),
                                                              "--build-dir",
                                                              build_dir.generic_string(),
                                                              "--",
                                                              sample_path.generic_string()},
                                                             build_dir / "bundle_stage_run_output.txt",
                                                             "bundle stage run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("phase264 bundle stage run should succeed:\n" + run_output);
    }
    ExpectOutputContains(run_output,
                         "bundle-bytes=5000\n",
                         "phase264 bundle stage should report the staged byte count");
    ExpectOutputContains(run_output,
                         "run-bytes=8192\n",
                         "phase264 bundle stage should report the page-rounded run capacity");
    ExpectOutputContains(run_output,
                         "granules=2\n",
                         "phase264 bundle stage should report the explicit run granule count");
    ExpectOutputContains(run_output,
                         "sum=" + std::to_string(sample_sum) + "\n",
                         "phase264 bundle stage should report the deterministic byte sum");

    const auto [test_outcome, test_output] = RunCommandCapture({mc_path.generic_string(),
                                                                "test",
                                                                "--project",
                                                                project_path.generic_string(),
                                                                "--build-dir",
                                                                build_dir.generic_string()},
                                                               build_dir / "bundle_stage_test_output.txt",
                                                               "bundle stage test");
    if (!test_outcome.exited || test_outcome.exit_code != 0) {
        Fail("phase264 bundle stage tests should pass:\n" + test_output);
    }
    ExpectOutputContains(test_output,
                         "PASS run_rounding_test.test_run_rounds_to_granules",
                         "phase264 bundle stage tests should cover run rounding");
    ExpectOutputContains(test_output,
                         "PASS stage_view_test.test_stage_view_tracks_used_bytes",
                         "phase264 bundle stage tests should cover used-byte tracking");
    ExpectOutputContains(test_output,
                         "2 tests, 2 passed, 0 failed",
                         "phase264 bundle stage test summary should be deterministic");
}

}  // namespace mc::tool_tests