#include <filesystem>
#include <string>
#include <string_view>

#include "tests/support/process_utils.h"
#include "tests/tool/tool_real_project_suite_internal.h"
#include "tests/tool/tool_suite_common.h"

namespace {

using mc::test_support::ExpectOutputContains;
using mc::test_support::Fail;
using mc::test_support::RunCommandCapture;
using mc::tool_tests::RunProjectTestAndExpectSuccess;

std::string RunWorkerQueueProjectAndExpectSuccess(const std::filesystem::path& mc_path,
                                                  const std::filesystem::path& project_path,
                                                  const std::filesystem::path& build_dir,
                                                  std::string_view output_name,
                                                  const std::string& context) {
    const auto [outcome, output] = RunCommandCapture({mc_path.generic_string(),
                                                      "run",
                                                      "--project",
                                                      project_path.generic_string(),
                                                      "--build-dir",
                                                      build_dir.generic_string()},
                                                     build_dir / std::string(output_name),
                                                     context);
    if (!outcome.exited || outcome.exit_code != 0) {
        Fail(context + " should pass:\n" + output);
    }
    return output;
}

void ExpectWorkerQueueRunOutput(std::string_view output,
                                const std::string& context_prefix) {
    ExpectOutputContains(output,
                         "worker-queue-ok\n",
                         context_prefix + ": should print the deterministic success line");
}

void ExpectWorkerQueueTestOutput(std::string_view output,
                                 const std::string& context_prefix) {
    ExpectOutputContains(output,
                         "testing target worker-queue",
                         context_prefix + ": should announce the target under test");
    ExpectOutputContains(output,
                         "ordinary tests target worker-queue: 2 cases, mode=checked, timeout=5000 ms",
                         context_prefix + ": should print ordinary test scope");
    ExpectOutputContains(output,
                         "PASS expected_sum_test.test_expected_sum",
                         context_prefix + ": should include expected-sum coverage");
    ExpectOutputContains(output,
                         "PASS next_slot_test.test_next_slot",
                         context_prefix + ": should include slot-wrap coverage");
    ExpectOutputContains(output,
                         "2 tests, 2 passed, 0 failed",
                         context_prefix + ": should print the deterministic summary");
    ExpectOutputContains(output,
                         "PASS ordinary tests for target worker-queue (2 cases)",
                         context_prefix + ": should print the ordinary-test verdict");
    ExpectOutputContains(output,
                         "PASS target worker-queue",
                         context_prefix + ": should print the target verdict");
}

}  // namespace

namespace mc::tool_tests {

void TestRealWorkerQueueProject(const std::filesystem::path& source_root,
                                const std::filesystem::path& binary_root,
                                const std::filesystem::path& mc_path) {
    const std::filesystem::path project_path = source_root / "examples/real/worker_queue/build.toml";
    const std::filesystem::path run_test_rerun_build_dir = binary_root / "worker_queue_run_test_rerun_build";
    std::filesystem::remove_all(run_test_rerun_build_dir);

    std::string run_output = RunWorkerQueueProjectAndExpectSuccess(mc_path,
                                                                   project_path,
                                                                   run_test_rerun_build_dir,
                                                                   "worker_queue_run_output.txt",
                                                                   "worker queue run before tests");
    ExpectWorkerQueueRunOutput(run_output,
                               "phase20 worker queue run before tests");

    std::string test_output = RunProjectTestAndExpectSuccess(mc_path,
                                                             project_path,
                                                             run_test_rerun_build_dir,
                                                             "worker_queue_test_output.txt",
                                                             "worker queue test after run");
    ExpectWorkerQueueTestOutput(test_output,
                                "phase20 worker queue test after run");

    run_output = RunWorkerQueueProjectAndExpectSuccess(mc_path,
                                                       project_path,
                                                       run_test_rerun_build_dir,
                                                       "worker_queue_rerun_output.txt",
                                                       "worker queue rerun after tests");
    ExpectWorkerQueueRunOutput(run_output,
                               "phase20 worker queue rerun after tests");

    const std::filesystem::path test_run_rerun_build_dir = binary_root / "worker_queue_test_run_rerun_build";
    std::filesystem::remove_all(test_run_rerun_build_dir);

    test_output = RunProjectTestAndExpectSuccess(mc_path,
                                                 project_path,
                                                 test_run_rerun_build_dir,
                                                 "worker_queue_initial_test_output.txt",
                                                 "worker queue initial test");
    ExpectWorkerQueueTestOutput(test_output,
                                "phase20 worker queue initial test");

    run_output = RunWorkerQueueProjectAndExpectSuccess(mc_path,
                                                       project_path,
                                                       test_run_rerun_build_dir,
                                                       "worker_queue_run_after_test_output.txt",
                                                       "worker queue run after tests");
    ExpectWorkerQueueRunOutput(run_output,
                               "phase20 worker queue run after tests");

    test_output = RunProjectTestAndExpectSuccess(mc_path,
                                                 project_path,
                                                 test_run_rerun_build_dir,
                                                 "worker_queue_retest_output.txt",
                                                 "worker queue retest after run");
    ExpectWorkerQueueTestOutput(test_output,
                                "phase20 worker queue retest after run");
}

}  // namespace mc::tool_tests