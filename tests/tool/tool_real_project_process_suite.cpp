#include <filesystem>
#include <string>
#include <string_view>

#include "tests/support/process_utils.h"
#include "tests/tool/tool_real_project_suite_internal.h"
#include "tests/tool/tool_suite_common.h"

namespace {

using mc::test_support::BackgroundProcess;
using mc::test_support::CloseFd;
using mc::test_support::ConnectLoopbackWithRetry;
using mc::test_support::ExpectBackgroundProcessSuccess;
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

std::string RunPipeHandoffProjectAndExpectSuccess(const std::filesystem::path& mc_path,
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

std::string RunPipeReadyProjectAndExpectSuccess(const std::filesystem::path& mc_path,
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

std::string RunLineFilterRelayProjectAndExpectSuccess(const std::filesystem::path& mc_path,
                                                      const std::filesystem::path& project_path,
                                                      const std::filesystem::path& build_dir,
                                                      std::string_view input_text,
                                                      std::string_view output_name,
                                                      const std::string& context) {
    const auto [outcome, output] = RunCommandCapture({mc_path.generic_string(),
                                                      "run",
                                                      "--project",
                                                      project_path.generic_string(),
                                                      "--build-dir",
                                                      build_dir.generic_string(),
                                                      "--",
                                                      std::string(input_text)},
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

void ExpectPipeHandoffRunOutput(std::string_view output,
                                const std::string& context_prefix) {
    ExpectOutputContains(output,
                         "pipe-handoff-ok\n",
                         context_prefix + ": should print the deterministic success line");
}

void ExpectPipeHandoffTestOutput(std::string_view output,
                                 const std::string& context_prefix) {
    ExpectOutputContains(output,
                         "testing target pipe-handoff",
                         context_prefix + ": should announce the target under test");
    ExpectOutputContains(output,
                         "ordinary tests target pipe-handoff: 2 cases, mode=checked, timeout=5000 ms",
                         context_prefix + ": should print ordinary test scope");
    ExpectOutputContains(output,
                         "PASS message_len_test.test_message_len",
                         context_prefix + ": should include message-length coverage");
    ExpectOutputContains(output,
                         "PASS message_matches_test.test_message_matches",
                         context_prefix + ": should include message-match coverage");
    ExpectOutputContains(output,
                         "2 tests, 2 passed, 0 failed",
                         context_prefix + ": should print the deterministic summary");
    ExpectOutputContains(output,
                         "PASS ordinary tests for target pipe-handoff (2 cases)",
                         context_prefix + ": should print the ordinary-test verdict");
    ExpectOutputContains(output,
                         "PASS target pipe-handoff",
                         context_prefix + ": should print the target verdict");
}

void ExpectPipeReadyRunOutput(std::string_view output,
                              const std::string& context_prefix) {
    ExpectOutputContains(output,
                         "pipe-ready-ok\n",
                         context_prefix + ": should print the deterministic success line");
}

void ExpectPipeReadyTestOutput(std::string_view output,
                               const std::string& context_prefix) {
    ExpectOutputContains(output,
                         "testing target pipe-ready",
                         context_prefix + ": should announce the target under test");
    ExpectOutputContains(output,
                         "ordinary tests target pipe-ready: 2 cases, mode=checked, timeout=5000 ms",
                         context_prefix + ": should print ordinary test scope");
    ExpectOutputContains(output,
                         "PASS message_len_test.test_message_len",
                         context_prefix + ": should include message-length coverage");
    ExpectOutputContains(output,
                         "PASS message_matches_test.test_message_matches",
                         context_prefix + ": should include message-match coverage");
    ExpectOutputContains(output,
                         "2 tests, 2 passed, 0 failed",
                         context_prefix + ": should print the deterministic summary");
    ExpectOutputContains(output,
                         "PASS ordinary tests for target pipe-ready (2 cases)",
                         context_prefix + ": should print the ordinary-test verdict");
    ExpectOutputContains(output,
                         "PASS target pipe-ready",
                         context_prefix + ": should print the target verdict");
}

void ExpectLineFilterRelayRunOutput(std::string_view output,
                                    const std::string& context_prefix) {
    ExpectOutputContains(output,
                         "PHASE FORTY FIVE\n",
                         context_prefix + ": should print the transformed child-process output");
}

void ExpectLineFilterRelayTestOutput(std::string_view output,
                                     const std::string& context_prefix) {
    ExpectOutputContains(output,
                         "testing target line-filter-relay",
                         context_prefix + ": should announce the target under test");
    ExpectOutputContains(output,
                         "ordinary tests target line-filter-relay: 4 cases, mode=checked, timeout=5000 ms",
                         context_prefix + ": should print ordinary test scope");
    ExpectOutputContains(output,
                         "PASS relay_child_diagnostics_test.test_relay_child_diagnostics",
                         context_prefix + ": should include merged child-diagnostics coverage");
    ExpectOutputContains(output,
                         "PASS relay_child_split_diagnostics_test.test_relay_child_split_diagnostics",
                         context_prefix + ": should include split child-diagnostics coverage");
    ExpectOutputContains(output,
                         "PASS relay_empty_line_test.test_relay_empty_line",
                         context_prefix + ": should include empty-line relay coverage");
    ExpectOutputContains(output,
                         "PASS relay_line_test.test_relay_line",
                         context_prefix + ": should include transformed-line relay coverage");
    ExpectOutputContains(output,
                         "4 tests, 4 passed, 0 failed",
                         context_prefix + ": should print the deterministic summary");
    ExpectOutputContains(output,
                         "PASS ordinary tests for target line-filter-relay (4 cases)",
                         context_prefix + ": should print the ordinary-test verdict");
    ExpectOutputContains(output,
                         "PASS target line-filter-relay",
                         context_prefix + ": should print the target verdict");
}


}  // namespace

namespace mc::tool_tests {

void TestRealWorkerQueueProject(const std::filesystem::path& source_root,
                                const std::filesystem::path& binary_root,
                                const std::filesystem::path& mc_path) {
    const std::filesystem::path worker_queue_project_path = source_root / "examples/real/worker_queue/build.toml";
    const std::filesystem::path worker_queue_run_test_rerun_build_dir = binary_root / "worker_queue_run_test_rerun_build";
    std::filesystem::remove_all(worker_queue_run_test_rerun_build_dir);

    std::string worker_queue_run_output = RunWorkerQueueProjectAndExpectSuccess(mc_path,
                                                                                worker_queue_project_path,
                                                                                worker_queue_run_test_rerun_build_dir,
                                                                                "worker_queue_run_output.txt",
                                                                                "worker queue run before tests");
    ExpectWorkerQueueRunOutput(worker_queue_run_output,
                               "phase20 worker queue run before tests");

    std::string worker_queue_test_output = RunProjectTestAndExpectSuccess(mc_path,
                                                                          worker_queue_project_path,
                                                                          worker_queue_run_test_rerun_build_dir,
                                                                          "worker_queue_test_output.txt",
                                                                          "worker queue test after run");
    ExpectWorkerQueueTestOutput(worker_queue_test_output,
                                "phase20 worker queue test after run");

    worker_queue_run_output = RunWorkerQueueProjectAndExpectSuccess(mc_path,
                                                                    worker_queue_project_path,
                                                                    worker_queue_run_test_rerun_build_dir,
                                                                    "worker_queue_rerun_output.txt",
                                                                    "worker queue rerun after tests");
    ExpectWorkerQueueRunOutput(worker_queue_run_output,
                               "phase20 worker queue rerun after tests");

    const std::filesystem::path worker_queue_test_run_rerun_build_dir = binary_root / "worker_queue_test_run_rerun_build";
    std::filesystem::remove_all(worker_queue_test_run_rerun_build_dir);

    worker_queue_test_output = RunProjectTestAndExpectSuccess(mc_path,
                                                              worker_queue_project_path,
                                                              worker_queue_test_run_rerun_build_dir,
                                                              "worker_queue_initial_test_output.txt",
                                                              "worker queue initial test");
    ExpectWorkerQueueTestOutput(worker_queue_test_output,
                                "phase20 worker queue initial test");

    worker_queue_run_output = RunWorkerQueueProjectAndExpectSuccess(mc_path,
                                                                    worker_queue_project_path,
                                                                    worker_queue_test_run_rerun_build_dir,
                                                                    "worker_queue_run_after_test_output.txt",
                                                                    "worker queue run after tests");
    ExpectWorkerQueueRunOutput(worker_queue_run_output,
                               "phase20 worker queue run after tests");

    worker_queue_test_output = RunProjectTestAndExpectSuccess(mc_path,
                                                              worker_queue_project_path,
                                                              worker_queue_test_run_rerun_build_dir,
                                                              "worker_queue_retest_output.txt",
                                                              "worker queue retest after run");
    ExpectWorkerQueueTestOutput(worker_queue_test_output,
                                "phase20 worker queue retest after run");
}

void TestRealPipeHandoffProject(const std::filesystem::path& source_root,
                                const std::filesystem::path& binary_root,
                                const std::filesystem::path& mc_path) {
    const std::filesystem::path pipe_handoff_project_path = source_root / "examples/real/pipe_handoff/build.toml";
    const std::filesystem::path pipe_handoff_run_test_rerun_build_dir = binary_root / "pipe_handoff_run_test_rerun_build";
    std::filesystem::remove_all(pipe_handoff_run_test_rerun_build_dir);

    std::string pipe_handoff_run_output = RunPipeHandoffProjectAndExpectSuccess(mc_path,
                                                                                pipe_handoff_project_path,
                                                                                pipe_handoff_run_test_rerun_build_dir,
                                                                                "pipe_handoff_run_output.txt",
                                                                                "pipe handoff run before tests");
    ExpectPipeHandoffRunOutput(pipe_handoff_run_output,
                               "phase43 pipe handoff run before tests");

    std::string pipe_handoff_test_output = RunProjectTestAndExpectSuccess(mc_path,
                                                                          pipe_handoff_project_path,
                                                                          pipe_handoff_run_test_rerun_build_dir,
                                                                          "pipe_handoff_test_output.txt",
                                                                          "pipe handoff test after run");
    ExpectPipeHandoffTestOutput(pipe_handoff_test_output,
                                "phase43 pipe handoff test after run");

    pipe_handoff_run_output = RunPipeHandoffProjectAndExpectSuccess(mc_path,
                                                                    pipe_handoff_project_path,
                                                                    pipe_handoff_run_test_rerun_build_dir,
                                                                    "pipe_handoff_rerun_output.txt",
                                                                    "pipe handoff rerun after tests");
    ExpectPipeHandoffRunOutput(pipe_handoff_run_output,
                               "phase43 pipe handoff rerun after tests");

    const std::filesystem::path pipe_handoff_test_run_rerun_build_dir = binary_root / "pipe_handoff_test_run_rerun_build";
    std::filesystem::remove_all(pipe_handoff_test_run_rerun_build_dir);

    pipe_handoff_test_output = RunProjectTestAndExpectSuccess(mc_path,
                                                              pipe_handoff_project_path,
                                                              pipe_handoff_test_run_rerun_build_dir,
                                                              "pipe_handoff_initial_test_output.txt",
                                                              "pipe handoff initial test");
    ExpectPipeHandoffTestOutput(pipe_handoff_test_output,
                                "phase43 pipe handoff initial test");

    pipe_handoff_run_output = RunPipeHandoffProjectAndExpectSuccess(mc_path,
                                                                    pipe_handoff_project_path,
                                                                    pipe_handoff_test_run_rerun_build_dir,
                                                                    "pipe_handoff_run_after_test_output.txt",
                                                                    "pipe handoff run after tests");
    ExpectPipeHandoffRunOutput(pipe_handoff_run_output,
                               "phase43 pipe handoff run after tests");

    pipe_handoff_test_output = RunProjectTestAndExpectSuccess(mc_path,
                                                              pipe_handoff_project_path,
                                                              pipe_handoff_test_run_rerun_build_dir,
                                                              "pipe_handoff_retest_output.txt",
                                                              "pipe handoff retest after run");
    ExpectPipeHandoffTestOutput(pipe_handoff_test_output,
                                "phase43 pipe handoff retest after run");
}

void TestRealPipeReadyProject(const std::filesystem::path& source_root,
                              const std::filesystem::path& binary_root,
                              const std::filesystem::path& mc_path) {
    const std::filesystem::path pipe_ready_project_path = source_root / "examples/real/pipe_ready/build.toml";
    const std::filesystem::path pipe_ready_run_test_rerun_build_dir = binary_root / "pipe_ready_run_test_rerun_build";
    std::filesystem::remove_all(pipe_ready_run_test_rerun_build_dir);

    std::string pipe_ready_run_output = RunPipeReadyProjectAndExpectSuccess(mc_path,
                                                                            pipe_ready_project_path,
                                                                            pipe_ready_run_test_rerun_build_dir,
                                                                            "pipe_ready_run_output.txt",
                                                                            "pipe ready run before tests");
    ExpectPipeReadyRunOutput(pipe_ready_run_output,
                             "phase43 pipe ready run before tests");

    std::string pipe_ready_test_output = RunProjectTestAndExpectSuccess(mc_path,
                                                                        pipe_ready_project_path,
                                                                        pipe_ready_run_test_rerun_build_dir,
                                                                        "pipe_ready_test_output.txt",
                                                                        "pipe ready test after run");
    ExpectPipeReadyTestOutput(pipe_ready_test_output,
                              "phase43 pipe ready test after run");

    pipe_ready_run_output = RunPipeReadyProjectAndExpectSuccess(mc_path,
                                                                pipe_ready_project_path,
                                                                pipe_ready_run_test_rerun_build_dir,
                                                                "pipe_ready_rerun_output.txt",
                                                                "pipe ready rerun after tests");
    ExpectPipeReadyRunOutput(pipe_ready_run_output,
                             "phase43 pipe ready rerun after tests");

    const std::filesystem::path pipe_ready_test_run_rerun_build_dir = binary_root / "pipe_ready_test_run_rerun_build";
    std::filesystem::remove_all(pipe_ready_test_run_rerun_build_dir);

    pipe_ready_test_output = RunProjectTestAndExpectSuccess(mc_path,
                                                            pipe_ready_project_path,
                                                            pipe_ready_test_run_rerun_build_dir,
                                                            "pipe_ready_initial_test_output.txt",
                                                            "pipe ready initial test");
    ExpectPipeReadyTestOutput(pipe_ready_test_output,
                              "phase43 pipe ready initial test");

    pipe_ready_run_output = RunPipeReadyProjectAndExpectSuccess(mc_path,
                                                                pipe_ready_project_path,
                                                                pipe_ready_test_run_rerun_build_dir,
                                                                "pipe_ready_run_after_test_output.txt",
                                                                "pipe ready run after tests");
    ExpectPipeReadyRunOutput(pipe_ready_run_output,
                             "phase43 pipe ready run after tests");

    pipe_ready_test_output = RunProjectTestAndExpectSuccess(mc_path,
                                                            pipe_ready_project_path,
                                                            pipe_ready_test_run_rerun_build_dir,
                                                            "pipe_ready_retest_output.txt",
                                                            "pipe ready retest after run");
    ExpectPipeReadyTestOutput(pipe_ready_test_output,
                              "phase43 pipe ready retest after run");
}

void TestRealLineFilterRelayProject(const std::filesystem::path& source_root,
                                    const std::filesystem::path& binary_root,
                                    const std::filesystem::path& mc_path) {
    const std::filesystem::path project_path = source_root / "examples/real/line_filter_relay/build.toml";
    const std::filesystem::path run_test_rerun_build_dir = binary_root / "line_filter_relay_run_test_rerun_build";
    std::filesystem::remove_all(run_test_rerun_build_dir);

    std::string run_output = RunLineFilterRelayProjectAndExpectSuccess(mc_path,
                                                                       project_path,
                                                                       run_test_rerun_build_dir,
                                                                       "phase forty five",
                                                                       "line_filter_relay_run_output.txt",
                                                                       "line filter relay run before tests");
    ExpectLineFilterRelayRunOutput(run_output,
                                   "phase46 line filter relay run before tests");

    std::string test_output = RunProjectTestAndExpectSuccess(mc_path,
                                                             project_path,
                                                             run_test_rerun_build_dir,
                                                             "line_filter_relay_test_output.txt",
                                                             "line filter relay test after run");
    ExpectLineFilterRelayTestOutput(test_output,
                                    "phase46 line filter relay test after run");

    run_output = RunLineFilterRelayProjectAndExpectSuccess(mc_path,
                                                           project_path,
                                                           run_test_rerun_build_dir,
                                                           "phase forty five",
                                                           "line_filter_relay_rerun_output.txt",
                                                           "line filter relay rerun after tests");
    ExpectLineFilterRelayRunOutput(run_output,
                                   "phase46 line filter relay rerun after tests");

    const std::filesystem::path test_run_rerun_build_dir = binary_root / "line_filter_relay_test_run_rerun_build";
    std::filesystem::remove_all(test_run_rerun_build_dir);

    test_output = RunProjectTestAndExpectSuccess(mc_path,
                                                 project_path,
                                                 test_run_rerun_build_dir,
                                                 "line_filter_relay_initial_test_output.txt",
                                                 "line filter relay initial test");
    ExpectLineFilterRelayTestOutput(test_output,
                                    "phase46 line filter relay initial test");

    run_output = RunLineFilterRelayProjectAndExpectSuccess(mc_path,
                                                           project_path,
                                                           test_run_rerun_build_dir,
                                                           "phase forty five",
                                                           "line_filter_relay_run_after_test_output.txt",
                                                           "line filter relay run after tests");
    ExpectLineFilterRelayRunOutput(run_output,
                                   "phase46 line filter relay run after tests");

    test_output = RunProjectTestAndExpectSuccess(mc_path,
                                                 project_path,
                                                 test_run_rerun_build_dir,
                                                 "line_filter_relay_retest_output.txt",
                                                 "line filter relay retest after run");
    ExpectLineFilterRelayTestOutput(test_output,
                                    "phase46 line filter relay retest after run");
}

}  // namespace mc::tool_tests