#include <filesystem>
#include <functional>
#include <string>
#include <string_view>

#include "compiler/support/dump_paths.h"
#include "tests/support/process_utils.h"
#include "tests/tool/tool_suite_common.h"

namespace {

using mc::test_support::BackgroundProcess;
using mc::test_support::CloseFd;
using mc::test_support::ConnectLoopbackWithRetry;
using mc::test_support::CopyDirectoryTree;
using mc::test_support::ExpectBackgroundProcessSuccess;
using mc::test_support::ExpectOutputContains;
using mc::test_support::Fail;
using mc::test_support::ReadExactFromSocket;
using mc::test_support::ReadFile;
using mc::test_support::RequireWriteTime;
using mc::test_support::ReserveLoopbackPort;
using mc::test_support::RunCommandCapture;
using mc::test_support::SleepForTimestampTick;
using mc::test_support::SpawnBackgroundCommand;
using mc::test_support::WriteAllToSocket;
using mc::test_support::WriteFile;
using mc::tool_tests::BuildProjectTargetAndCapture;
using mc::tool_tests::BuildProjectTargetAndExpectSuccess;
using mc::tool_tests::CheckProjectTargetAndExpectSuccess;
using mc::tool_tests::RunProjectTargetAndExpectFailure;
using mc::tool_tests::RunProjectTargetAndExpectSuccess;
using mc::tool_tests::RunProjectTestAndExpectSuccess;
using mc::tool_tests::RunProjectTestTargetAndExpectSuccess;

std::string BuildPartialWriteResponse(size_t size) {
    static constexpr std::string_view kPattern = "phase16-partial-write-state|";

    std::string payload;
    payload.resize(size);
    for (size_t index = 0; index < size; ++index) {
        payload[index] = kPattern[index % kPattern.size()];
    }
    return payload;
}

void ExerciseEventedEchoRoundTrip(const std::filesystem::path& mc_path,
                                  const std::filesystem::path& project_path,
                                  const std::filesystem::path& build_dir,
                                  uint16_t port,
                                  std::string_view output_name,
                                  const std::string& context) {
    const BackgroundProcess run_process = SpawnBackgroundCommand({mc_path.generic_string(),
                                                                  "run",
                                                                  "--project",
                                                                  project_path.generic_string(),
                                                                  "--build-dir",
                                                                  build_dir.generic_string(),
                                                                  "--",
                                                                  std::to_string(port)},
                                                                 build_dir / std::string(output_name),
                                                                 context);

    const std::string request = "phase13-echo";
    const int client_fd = ConnectLoopbackWithRetry(port, 3000, "connect to " + context);
    WriteAllToSocket(client_fd, request, "write request to " + context);
    const std::string reply = ReadExactFromSocket(client_fd,
                                                  request.size(),
                                                  "read reply from " + context);
    CloseFd(client_fd);
    if (reply != request) {
        Fail(context + ": server returned unexpected payload: '" + reply + "'");
    }

    ExpectBackgroundProcessSuccess(run_process, 3000, "wait for " + context);
}

void ExercisePartialWriteRoundTrip(const std::filesystem::path& mc_path,
                                   const std::filesystem::path& project_path,
                                   const std::filesystem::path& build_dir,
                                   uint16_t port,
                                   std::string_view output_name,
                                   const std::string& context) {
    const BackgroundProcess run_process = SpawnBackgroundCommand({mc_path.generic_string(),
                                                                  "run",
                                                                  "--project",
                                                                  project_path.generic_string(),
                                                                  "--build-dir",
                                                                  build_dir.generic_string(),
                                                                  "--",
                                                                  std::to_string(port)},
                                                                 build_dir / std::string(output_name),
                                                                 context);

    const int client_fd = ConnectLoopbackWithRetry(port, 3000, "connect to " + context);
    WriteAllToSocket(client_fd, "push", "write request to " + context);
    const std::string reply = ReadExactFromSocket(client_fd,
                                                  1536,
                                                  "read reply from " + context);
    if (reply != BuildPartialWriteResponse(1536)) {
        CloseFd(client_fd);
        Fail(context + ": server returned unexpected partial-write payload");
    }

    WriteAllToSocket(client_fd, "!", "write ack to " + context);
    CloseFd(client_fd);

    ExpectBackgroundProcessSuccess(run_process, 3000, "wait for " + context);
}

void ExpectReviewBoardRunOutput(std::string_view output,
                                std::string_view expected_line,
                                const std::string& context_prefix) {
    ExpectOutputContains(output,
                         expected_line,
                         context_prefix + ": should print the deterministic target result");
}

void ExpectReviewBoardTestOutput(std::string_view output,
                                 std::string_view target_name,
                                 const std::string& context_prefix) {
    const std::string target_text(target_name);
    ExpectOutputContains(output,
                         "testing target " + target_text,
                         context_prefix + ": should announce the target under test");
    ExpectOutputContains(output,
                         "ordinary tests target " + target_text + ": 3 cases, mode=checked, timeout=5000 ms",
                         context_prefix + ": should print ordinary test scope");
    ExpectOutputContains(output,
                         "PASS audit_pause_test.test_audit_pause",
                         context_prefix + ": should include audit pause coverage");
    ExpectOutputContains(output,
                         "PASS focus_threshold_test.test_focus_threshold",
                         context_prefix + ": should include focus-threshold coverage");
    ExpectOutputContains(output,
                         "PASS review_scan_test.test_review_scan",
                         context_prefix + ": should include shared scanner coverage");
    ExpectOutputContains(output,
                         "3 tests, 3 passed, 0 failed",
                         context_prefix + ": should print the deterministic summary");
    ExpectOutputContains(output,
                         "PASS ordinary tests for target " + target_text + " (3 cases)",
                         context_prefix + ": should print the ordinary-test verdict");
    ExpectOutputContains(output,
                         "PASS target " + target_text,
                         context_prefix + ": should print the target verdict");
}

void ExpectIssueRollupRunOutput(std::string_view output,
                                std::string_view expected_line,
                                const std::string& context_prefix) {
    ExpectOutputContains(output,
                         expected_line,
                         context_prefix + ": should print the deterministic grouped-package result");
}

void ExpectIssueRollupTestOutput(std::string_view output,
                                 const std::string& context_prefix) {
    ExpectOutputContains(output,
                         "testing target issue-rollup",
                         context_prefix + ": should announce the grouped-root target under test");
    ExpectOutputContains(output,
                         "ordinary tests target issue-rollup: 3 cases, mode=checked, timeout=5000 ms",
                         context_prefix + ": should print the ordinary test scope");
    ExpectOutputContains(output,
                         "PASS model_total_items_test.test_model_total_items",
                         context_prefix + ": should include model-package coverage");
    ExpectOutputContains(output,
                         "PASS parse_summary_test.test_parse_summary",
                         context_prefix + ": should include parse-package coverage");
    ExpectOutputContains(output,
                         "PASS render_kind_test.test_render_kind",
                         context_prefix + ": should include render-package coverage");
    ExpectOutputContains(output,
                         "3 tests, 3 passed, 0 failed",
                         context_prefix + ": should print the deterministic summary");
    ExpectOutputContains(output,
                         "PASS ordinary tests for target issue-rollup (3 cases)",
                         context_prefix + ": should print the ordinary-test verdict");
    ExpectOutputContains(output,
                         "PASS target issue-rollup",
                         context_prefix + ": should print the target verdict");
}

void ExpectEventedEchoTestOutput(std::string_view output,
                                 const std::string& context_prefix) {
    ExpectOutputContains(output,
                         "testing target echo",
                         context_prefix + ": should announce the target under test");
    ExpectOutputContains(output,
                         "ordinary tests target echo: 3 cases, mode=checked, timeout=5000 ms",
                         context_prefix + ": should print ordinary test scope");
    ExpectOutputContains(output,
                         "PASS parse_valid_port_test.test_parse_valid_port",
                         context_prefix + ": should include valid-port coverage");
    ExpectOutputContains(output,
                         "PASS parse_zero_port_test.test_parse_zero_port",
                         context_prefix + ": should include zero-port coverage");
    ExpectOutputContains(output,
                         "PASS parse_invalid_port_test.test_parse_invalid_port",
                         context_prefix + ": should include invalid-port coverage");
    ExpectOutputContains(output,
                         "3 tests, 3 passed, 0 failed",
                         context_prefix + ": should print the deterministic summary");
    ExpectOutputContains(output,
                         "PASS ordinary tests for target echo (3 cases)",
                         context_prefix + ": should print the ordinary test verdict");
    ExpectOutputContains(output,
                         "PASS target echo",
                         context_prefix + ": should print the target verdict");
}

void ExpectPartialWriteTestOutput(std::string_view output,
                                  const std::string& context_prefix) {
    ExpectOutputContains(output,
                         "testing target partial-write",
                         context_prefix + ": should announce the target under test");
    ExpectOutputContains(output,
                         "ordinary tests target partial-write: 2 cases, mode=checked, timeout=5000 ms",
                         context_prefix + ": should print ordinary test scope");
    ExpectOutputContains(output,
                         "PASS fill_response_pattern_test.test_fill_response_pattern",
                         context_prefix + ": should include response-pattern coverage");
    ExpectOutputContains(output,
                         "PASS write_chunk_len_test.test_write_chunk_len",
                         context_prefix + ": should include write-window coverage");
    ExpectOutputContains(output,
                         "2 tests, 2 passed, 0 failed",
                         context_prefix + ": should print the deterministic summary");
    ExpectOutputContains(output,
                         "PASS ordinary tests for target partial-write (2 cases)",
                         context_prefix + ": should print the ordinary test verdict");
    ExpectOutputContains(output,
                         "PASS target partial-write",
                         context_prefix + ": should print the target verdict");
}

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
                         context_prefix + ": should print the ordinary test verdict");
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

void ExerciseProjectRunTestRerunSequence(const std::filesystem::path& mc_path,
                                         const std::filesystem::path& project_path,
                                         const std::filesystem::path& build_dir,
                                         const std::function<void(uint16_t, std::string_view, const std::string&)>& run_round_trip,
                                         const std::function<void(std::string_view, const std::string&)>& verify_test_output,
                                         std::string_view first_run_output_name,
                                         std::string_view test_output_name,
                                         std::string_view second_run_output_name,
                                         const std::string& context_prefix) {
    run_round_trip(ReserveLoopbackPort(), first_run_output_name, context_prefix + " run before tests");
    const std::string test_output = RunProjectTestAndExpectSuccess(mc_path,
                                                                   project_path,
                                                                   build_dir,
                                                                   test_output_name,
                                                                   context_prefix + " test after run");
    verify_test_output(test_output, context_prefix + " test after run");
    run_round_trip(ReserveLoopbackPort(), second_run_output_name, context_prefix + " rerun after tests");
}

void ExerciseProjectTestRunRerunSequence(const std::filesystem::path& mc_path,
                                         const std::filesystem::path& project_path,
                                         const std::filesystem::path& build_dir,
                                         const std::function<void(uint16_t, std::string_view, const std::string&)>& run_round_trip,
                                         const std::function<void(std::string_view, const std::string&)>& verify_test_output,
                                         std::string_view first_test_output_name,
                                         std::string_view run_output_name,
                                         std::string_view second_test_output_name,
                                         const std::string& context_prefix) {
    const std::string first_test_output = RunProjectTestAndExpectSuccess(mc_path,
                                                                         project_path,
                                                                         build_dir,
                                                                         first_test_output_name,
                                                                         context_prefix + " initial test");
    verify_test_output(first_test_output, context_prefix + " initial test");
    run_round_trip(ReserveLoopbackPort(), run_output_name, context_prefix + " run after tests");
    const std::string second_test_output = RunProjectTestAndExpectSuccess(mc_path,
                                                                          project_path,
                                                                          build_dir,
                                                                          second_test_output_name,
                                                                          context_prefix + " retest after run");
    verify_test_output(second_test_output, context_prefix + " retest after run");
}

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

void TestRealEventedEchoProject(const std::filesystem::path& source_root,
                                const std::filesystem::path& binary_root,
                                const std::filesystem::path& mc_path) {
    const std::filesystem::path project_path = source_root / "examples/real/evented_echo/build.toml";
    const std::filesystem::path run_test_rerun_build_dir = binary_root / "evented_echo_run_test_rerun_build";
    std::filesystem::remove_all(run_test_rerun_build_dir);
    ExerciseProjectRunTestRerunSequence(mc_path,
                                        project_path,
                                        run_test_rerun_build_dir,
                                        [&](uint16_t port, std::string_view output_name, const std::string& context) {
                                            ExerciseEventedEchoRoundTrip(mc_path,
                                                                         project_path,
                                                                         run_test_rerun_build_dir,
                                                                         port,
                                                                         output_name,
                                                                         context);
                                        },
                                        ExpectEventedEchoTestOutput,
                                        "evented_echo_run_output.txt",
                                        "evented_echo_test_output.txt",
                                        "evented_echo_rerun_output.txt",
                                        "evented echo run-test-rerun");

    const std::filesystem::path test_run_rerun_build_dir = binary_root / "evented_echo_test_run_rerun_build";
    std::filesystem::remove_all(test_run_rerun_build_dir);
    ExerciseProjectTestRunRerunSequence(mc_path,
                                        project_path,
                                        test_run_rerun_build_dir,
                                        [&](uint16_t port, std::string_view output_name, const std::string& context) {
                                            ExerciseEventedEchoRoundTrip(mc_path,
                                                                         project_path,
                                                                         test_run_rerun_build_dir,
                                                                         port,
                                                                         output_name,
                                                                         context);
                                        },
                                        ExpectEventedEchoTestOutput,
                                        "evented_echo_initial_test_output.txt",
                                        "evented_echo_run_after_test_output.txt",
                                        "evented_echo_retest_output.txt",
                                        "evented echo test-run-rerun");

    const std::filesystem::path partial_write_project_path = source_root / "examples/real/evented_partial_write/build.toml";
    const std::filesystem::path partial_write_run_test_rerun_build_dir = binary_root / "evented_partial_write_run_test_rerun_build";
    std::filesystem::remove_all(partial_write_run_test_rerun_build_dir);
    ExerciseProjectRunTestRerunSequence(mc_path,
                                        partial_write_project_path,
                                        partial_write_run_test_rerun_build_dir,
                                        [&](uint16_t port, std::string_view output_name, const std::string& context) {
                                            ExercisePartialWriteRoundTrip(mc_path,
                                                                          partial_write_project_path,
                                                                          partial_write_run_test_rerun_build_dir,
                                                                          port,
                                                                          output_name,
                                                                          context);
                                        },
                                        ExpectPartialWriteTestOutput,
                                        "evented_partial_write_run_output.txt",
                                        "evented_partial_write_test_output.txt",
                                        "evented_partial_write_rerun_output.txt",
                                        "evented partial-write run-test-rerun");

    const std::filesystem::path partial_write_test_run_rerun_build_dir = binary_root / "evented_partial_write_test_run_rerun_build";
    std::filesystem::remove_all(partial_write_test_run_rerun_build_dir);
    ExerciseProjectTestRunRerunSequence(mc_path,
                                        partial_write_project_path,
                                        partial_write_test_run_rerun_build_dir,
                                        [&](uint16_t port, std::string_view output_name, const std::string& context) {
                                            ExercisePartialWriteRoundTrip(mc_path,
                                                                          partial_write_project_path,
                                                                          partial_write_test_run_rerun_build_dir,
                                                                          port,
                                                                          output_name,
                                                                          context);
                                        },
                                        ExpectPartialWriteTestOutput,
                                        "evented_partial_write_initial_test_output.txt",
                                        "evented_partial_write_run_after_test_output.txt",
                                        "evented_partial_write_retest_output.txt",
                                        "evented partial-write test-run-rerun");
}

void TestRealReviewBoardProject(const std::filesystem::path& source_root,
                                const std::filesystem::path& binary_root,
                                const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = source_root / "examples/real/review_board";
    const std::filesystem::path project_path = project_root / "build.toml";
    const std::filesystem::path sample_path = project_root / "tests/board_sample.txt";
    const std::filesystem::path graph_build_dir = binary_root / "review_board_graph_build";
    std::filesystem::remove_all(graph_build_dir);

    std::string audit_run_output = RunProjectTargetAndExpectSuccess(mc_path,
                                                                    project_path,
                                                                    graph_build_dir,
                                                                    "",
                                                                    sample_path,
                                                                    "review_board_audit_run_output.txt",
                                                                    "review board default-target run");
    ExpectReviewBoardRunOutput(audit_run_output,
                               "review-audit-pause\n",
                               "phase22 review board default-target run");

    std::string focus_run_output = RunProjectTargetAndExpectSuccess(mc_path,
                                                                    project_path,
                                                                    graph_build_dir,
                                                                    "focus",
                                                                    sample_path,
                                                                    "review_board_focus_run_output.txt",
                                                                    "review board explicit focus run");
    ExpectReviewBoardRunOutput(focus_run_output,
                               "review-focus-normal\n",
                               "phase22 review board explicit focus run");

    std::string audit_test_output = RunProjectTestTargetAndExpectSuccess(mc_path,
                                                                         project_path,
                                                                         graph_build_dir,
                                                                         "",
                                                                         "review_board_audit_test_output.txt",
                                                                         "review board default-target tests");
    ExpectReviewBoardTestOutput(audit_test_output,
                                "audit",
                                "phase22 review board default-target tests");

    std::string focus_test_output = RunProjectTestTargetAndExpectSuccess(mc_path,
                                                                         project_path,
                                                                         graph_build_dir,
                                                                         "focus",
                                                                         "review_board_focus_test_output.txt",
                                                                         "review board explicit focus tests");
    ExpectReviewBoardTestOutput(focus_test_output,
                                "focus",
                                "phase22 review board explicit focus tests");

    audit_run_output = RunProjectTargetAndExpectSuccess(mc_path,
                                                        project_path,
                                                        graph_build_dir,
                                                        "",
                                                        sample_path,
                                                        "review_board_audit_rerun_output.txt",
                                                        "review board default-target rerun");
    ExpectReviewBoardRunOutput(audit_run_output,
                               "review-audit-pause\n",
                               "phase22 review board default-target rerun");

    focus_run_output = RunProjectTargetAndExpectSuccess(mc_path,
                                                        project_path,
                                                        graph_build_dir,
                                                        "focus",
                                                        sample_path,
                                                        "review_board_focus_rerun_output.txt",
                                                        "review board explicit focus rerun");
    ExpectReviewBoardRunOutput(focus_run_output,
                               "review-focus-normal\n",
                               "phase22 review board explicit focus rerun");

    const std::filesystem::path cloned_project_root = binary_root / "review_board_clone";
    CopyDirectoryTree(project_root, cloned_project_root);
    WriteFile(cloned_project_root / "build.toml",
              "schema = 1\n"
              "project = \"review-board\"\n"
              "default = \"audit\"\n"
              "\n"
              "[targets.audit]\n"
              "kind = \"exe\"\n"
              "package = \"review-board\"\n"
              "root = \"src/audit_main.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"hosted\"\n"
              "\n"
              "[targets.audit.search_paths]\n"
              "modules = [\"src\", \"" + (source_root / "stdlib").generic_string() + "\"]\n"
              "\n"
              "[targets.audit.runtime]\n"
              "startup = \"default\"\n"
              "\n"
              "[targets.audit.tests]\n"
              "enabled = true\n"
              "roots = [\"tests\"]\n"
              "mode = \"checked\"\n"
              "timeout_ms = 5000\n"
              "\n"
              "[targets.focus]\n"
              "kind = \"exe\"\n"
              "package = \"review-board\"\n"
              "root = \"src/focus_main.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"hosted\"\n"
              "\n"
              "[targets.focus.search_paths]\n"
              "modules = [\"src\", \"" + (source_root / "stdlib").generic_string() + "\"]\n"
              "\n"
              "[targets.focus.runtime]\n"
              "startup = \"default\"\n"
              "\n"
              "[targets.focus.tests]\n"
              "enabled = true\n"
              "roots = [\"tests\"]\n"
              "mode = \"checked\"\n"
              "timeout_ms = 5000\n");
    const std::filesystem::path cloned_project_path = cloned_project_root / "build.toml";
    const std::filesystem::path cloned_sample_path = cloned_project_root / "tests/board_sample.txt";
    const std::filesystem::path rebuild_build_dir = binary_root / "review_board_rebuild_build";
    std::filesystem::remove_all(rebuild_build_dir);

    BuildProjectTargetAndExpectSuccess(mc_path,
                                       cloned_project_path,
                                       rebuild_build_dir,
                                       "",
                                       "review_board_initial_audit_build.txt",
                                       "review board initial audit build");
    BuildProjectTargetAndExpectSuccess(mc_path,
                                       cloned_project_path,
                                       rebuild_build_dir,
                                       "focus",
                                       "review_board_initial_focus_build.txt",
                                       "review board initial focus build");

    const auto review_scan_object = mc::support::ComputeBuildArtifactTargets(cloned_project_root / "src/review_scan.mc",
                                                                             rebuild_build_dir)
                                        .object;
    const auto internal_object = mc::support::ComputeBuildArtifactTargets(cloned_project_root / "src/internal.mc",
                                                                          rebuild_build_dir)
                                    .object;
    const auto review_status_object = mc::support::ComputeBuildArtifactTargets(cloned_project_root / "src/review_status.mc",
                                                                               rebuild_build_dir)
                                          .object;
    const auto audit_main_object = mc::support::ComputeBuildArtifactTargets(cloned_project_root / "src/audit_main.mc",
                                                                            rebuild_build_dir)
                                       .object;
    const auto focus_main_object = mc::support::ComputeBuildArtifactTargets(cloned_project_root / "src/focus_main.mc",
                                                                            rebuild_build_dir)
                                       .object;
    const auto review_scan_mci = mc::support::ComputeDumpTargets(cloned_project_root / "src/review_scan.mc",
                                                                 rebuild_build_dir)
                                      .mci;
    const auto internal_mci = mc::support::ComputeDumpTargets(cloned_project_root / "src/internal.mc",
                                                              rebuild_build_dir)
                                  .mci;
    const auto review_status_mci = mc::support::ComputeDumpTargets(cloned_project_root / "src/review_status.mc",
                                                                   rebuild_build_dir)
                                        .mci;
    const auto audit_executable = mc::support::ComputeBuildArtifactTargets(cloned_project_root / "src/audit_main.mc",
                                                                           rebuild_build_dir)
                                      .executable;
    const auto focus_executable = mc::support::ComputeBuildArtifactTargets(cloned_project_root / "src/focus_main.mc",
                                                                           rebuild_build_dir)
                                      .executable;

    const auto review_scan_object_time_1 = RequireWriteTime(review_scan_object);
    const auto internal_object_time_1 = RequireWriteTime(internal_object);
    const auto review_status_object_time_1 = RequireWriteTime(review_status_object);
    const auto audit_main_object_time_1 = RequireWriteTime(audit_main_object);
    const auto focus_main_object_time_1 = RequireWriteTime(focus_main_object);
    const auto review_scan_mci_time_1 = RequireWriteTime(review_scan_mci);
    const auto internal_mci_time_1 = RequireWriteTime(internal_mci);
    const auto review_status_mci_time_1 = RequireWriteTime(review_status_mci);
    const std::string review_scan_mci_text_1 = ReadFile(review_scan_mci);
    const std::string internal_mci_text_1 = ReadFile(internal_mci);
    const std::string review_status_mci_text_1 = ReadFile(review_status_mci);
    const auto audit_executable_time_1 = RequireWriteTime(audit_executable);
    const auto focus_executable_time_1 = RequireWriteTime(focus_executable);

    SleepForTimestampTick();
    BuildProjectTargetAndExpectSuccess(mc_path,
                                       cloned_project_path,
                                       rebuild_build_dir,
                                       "",
                                       "review_board_noop_audit_build.txt",
                                       "review board no-op audit build");
    BuildProjectTargetAndExpectSuccess(mc_path,
                                       cloned_project_path,
                                       rebuild_build_dir,
                                       "focus",
                                       "review_board_noop_focus_build.txt",
                                       "review board no-op focus build");

    if (RequireWriteTime(review_scan_object) != review_scan_object_time_1 ||
        RequireWriteTime(internal_object) != internal_object_time_1 ||
        RequireWriteTime(review_status_object) != review_status_object_time_1 ||
        RequireWriteTime(audit_main_object) != audit_main_object_time_1 ||
        RequireWriteTime(focus_main_object) != focus_main_object_time_1 ||
        RequireWriteTime(audit_executable) != audit_executable_time_1 ||
        RequireWriteTime(focus_executable) != focus_executable_time_1) {
        Fail("phase22 no-op rebuild should reuse both target executables and all shared-module objects");
    }
    if (ReadFile(review_scan_mci) != review_scan_mci_text_1 ||
        ReadFile(internal_mci) != internal_mci_text_1 ||
        ReadFile(review_status_mci) != review_status_mci_text_1) {
        Fail("phase22 no-op rebuild should preserve shared-module interface artifact contents");
    }

    SleepForTimestampTick();
    WriteFile(cloned_project_root / "src/internal.mc",
              "import strings\n"
              "\n"
              "@private\n"
              "func line_is_open(bytes: Slice<u8>, start: usize, newline: usize) bool {\n"
              "    if newline <= start {\n"
              "        return false\n"
              "    }\n"
              "    return bytes[start] == 79\n"
              "}\n"
              "\n"
              "@private\n"
              "func line_is_closed(bytes: Slice<u8>, start: usize, newline: usize) bool {\n"
              "    if newline <= start {\n"
              "        return false\n"
              "    }\n"
              "    return bytes[start] == 67\n"
              "}\n"
              "\n"
              "@private\n"
              "func line_is_urgent_open(bytes: Slice<u8>, start: usize, newline: usize) bool {\n"
              "    if newline <= start {\n"
              "        return false\n"
              "    }\n"
              "    if bytes[start] != 79 {\n"
              "        return false\n"
              "    }\n"
              "    return true\n"
              "}\n"
              "\n"
              "func count_open_items(text: str) usize {\n"
              "    bytes: Slice<u8> = strings.bytes(text)\n"
              "    count: usize = 0\n"
              "    start: usize = 0\n"
              "    while start < text.len {\n"
              "        newline: usize = start\n"
              "        while newline < text.len {\n"
              "            if bytes[newline] == 10 {\n"
              "                break\n"
              "            }\n"
              "            newline = newline + 1\n"
              "        }\n"
              "\n"
              "        if line_is_open(bytes, start, newline) {\n"
              "            count = count + 1\n"
              "        }\n"
              "\n"
              "        if newline == text.len {\n"
              "            break\n"
              "        }\n"
              "        start = newline + 1\n"
              "    }\n"
              "    return count\n"
              "}\n"
              "\n"
              "func count_closed_items(text: str) usize {\n"
              "    bytes: Slice<u8> = strings.bytes(text)\n"
              "    count: usize = 0\n"
              "    start: usize = 0\n"
              "    while start < text.len {\n"
              "        newline: usize = start\n"
              "        while newline < text.len {\n"
              "            if bytes[newline] == 10 {\n"
              "                break\n"
              "            }\n"
              "            newline = newline + 1\n"
              "        }\n"
              "\n"
              "        if line_is_closed(bytes, start, newline) {\n"
              "            count = count + 1\n"
              "        }\n"
              "\n"
              "        if newline == text.len {\n"
              "            break\n"
              "        }\n"
              "        start = newline + 1\n"
              "    }\n"
              "    return count\n"
              "}\n"
              "\n"
              "func count_urgent_open_items(text: str) usize {\n"
              "    bytes: Slice<u8> = strings.bytes(text)\n"
              "    count: usize = 0\n"
              "    start: usize = 0\n"
              "    while start < text.len {\n"
              "        newline: usize = start\n"
              "        while newline < text.len {\n"
              "            if bytes[newline] == 10 {\n"
              "                break\n"
              "            }\n"
              "            newline = newline + 1\n"
              "        }\n"
              "\n"
              "        if line_is_urgent_open(bytes, start, newline) {\n"
              "            count = count + 1\n"
              "        }\n"
              "\n"
              "        if newline == text.len {\n"
              "            break\n"
              "        }\n"
              "        start = newline + 1\n"
              "    }\n"
              "    return count\n"
              "}\n");

    BuildProjectTargetAndExpectSuccess(mc_path,
                                       cloned_project_path,
                                       rebuild_build_dir,
                                       "",
                                       "review_board_impl_audit_build.txt",
                                       "review board implementation-only audit rebuild");

    const auto review_scan_object_time_2 = RequireWriteTime(review_scan_object);
    const auto internal_object_time_2 = RequireWriteTime(internal_object);
    const auto review_status_object_time_2 = RequireWriteTime(review_status_object);
    const auto audit_main_object_time_2 = RequireWriteTime(audit_main_object);
    const auto focus_main_object_time_2 = RequireWriteTime(focus_main_object);
    const auto review_scan_mci_time_2 = RequireWriteTime(review_scan_mci);
    const auto internal_mci_time_2 = RequireWriteTime(internal_mci);
    const auto review_status_mci_time_2 = RequireWriteTime(review_status_mci);
    const std::string review_scan_mci_text_2 = ReadFile(review_scan_mci);
    const std::string internal_mci_text_2 = ReadFile(internal_mci);
    const std::string review_status_mci_text_2 = ReadFile(review_status_mci);
    const auto audit_executable_time_2 = RequireWriteTime(audit_executable);

    if (review_scan_object_time_2 != review_scan_object_time_1) {
        Fail("phase22 implementation-only internal scan edit should preserve the wrapper shared object");
    }
    if (!(internal_object_time_2 > internal_object_time_1)) {
        Fail("phase22 implementation-only internal scan edit should rebuild the deep internal object");
    }
    if (review_status_object_time_2 != review_status_object_time_1) {
        Fail("phase22 implementation-only internal scan edit should not rebuild the dependent shared status object");
    }
    if (audit_main_object_time_2 != audit_main_object_time_1) {
        Fail("phase22 implementation-only internal scan edit should not rebuild the default-target main object");
    }
    if (focus_main_object_time_2 != focus_main_object_time_1) {
        Fail("phase22 implementation-only internal scan edit should not rebuild the explicit-target main object");
    }
    if (review_scan_mci_text_2 != review_scan_mci_text_1) {
        Fail("phase22 implementation-only internal scan edit should preserve the wrapper shared interface artifact contents");
    }
    if (internal_mci_text_2 != internal_mci_text_1) {
        Fail("phase22 implementation-only internal scan edit should preserve the deep internal interface artifact contents");
    }
    if (review_status_mci_text_2 != review_status_mci_text_1) {
        Fail("phase22 implementation-only internal scan edit should preserve the dependent shared interface artifact contents");
    }
    if (!(audit_executable_time_2 > audit_executable_time_1)) {
        Fail("phase22 implementation-only internal scan edit should relink the default-target executable");
    }
    if (RequireWriteTime(focus_executable) != focus_executable_time_1) {
        Fail("phase22 implementation-only audit rebuild should not touch the explicit-target executable before it is selected");
    }

    SleepForTimestampTick();
    BuildProjectTargetAndExpectSuccess(mc_path,
                                       cloned_project_path,
                                       rebuild_build_dir,
                                       "focus",
                                       "review_board_impl_focus_build.txt",
                                       "review board implementation-only focus rebuild");

    if (RequireWriteTime(review_scan_object) != review_scan_object_time_2) {
        Fail("phase22 implementation-only focus rebuild should reuse the wrapper shared object");
    }
    if (RequireWriteTime(internal_object) != internal_object_time_2) {
        Fail("phase22 implementation-only focus rebuild should reuse the already rebuilt deep internal object");
    }
    if (RequireWriteTime(review_status_object) != review_status_object_time_1) {
        Fail("phase22 implementation-only focus rebuild should still reuse the dependent shared status object");
    }
    if (RequireWriteTime(focus_main_object) != focus_main_object_time_1) {
        Fail("phase22 implementation-only focus rebuild should reuse the explicit-target main object");
    }
    if (!(RequireWriteTime(focus_executable) > focus_executable_time_1)) {
        Fail("phase22 implementation-only focus rebuild should relink the explicit-target executable");
    }

    const auto [impl_focus_outcome, impl_focus_output] = RunCommandCapture({focus_executable.generic_string(),
                                                                            cloned_sample_path.generic_string()},
                                                                           rebuild_build_dir /
                                                                               "review_board_impl_focus_run.txt",
                                                                           "review board implementation-only focus executable run");
    if (!impl_focus_outcome.exited || impl_focus_outcome.exit_code != 0) {
        Fail("phase22 implementation-only focus executable run should pass:\n" + impl_focus_output);
    }
    ExpectReviewBoardRunOutput(impl_focus_output,
                               "review-focus-escalate\n",
                               "phase22 review board implementation-only focus executable run");

    const auto focus_executable_time_2 = RequireWriteTime(focus_executable);

    SleepForTimestampTick();
    WriteFile(cloned_project_root / "src/review_status.mc",
              "import fs\n"
              "import io\n"
              "import mem\n"
              "import review_scan\n"
              "\n"
              "func buffer_text(buf: *Buffer<u8>) str {\n"
              "    bytes: Slice<u8> = mem.slice_from_buffer<u8>(buf)\n"
              "    return str{ ptr: bytes.ptr, len: bytes.len }\n"
              "}\n"
              "\n"
              "func load_review_text(path: str) *Buffer<u8> {\n"
              "    return fs.read_all(path, mem.default_allocator())\n"
              "}\n"
              "\n"
              "func audit_should_pause_text(text: str) bool {\n"
              "    return review_scan.count_open_items(text) > review_scan.count_closed_items(text)\n"
              "}\n"
              "\n"
              "func focus_needs_escalation_text(text: str) bool {\n"
              "    return review_scan.count_urgent_open_items(text) > 2\n"
              "}\n"
              "\n"
              "func helper_version() i32 {\n"
              "    return 22\n"
              "}\n"
              "\n"
              "func run_audit(path: str) i32 {\n"
              "    text_buf: *Buffer<u8> = load_review_text(path)\n"
              "    if text_buf == nil {\n"
              "        return 91\n"
              "    }\n"
              "    defer mem.buffer_free<u8>(text_buf)\n"
              "\n"
              "    if audit_should_pause_text(buffer_text(text_buf)) {\n"
              "        if io.write_line(\"review-audit-pause\") != 0 {\n"
              "            return 92\n"
              "        }\n"
              "        return 0\n"
              "    }\n"
              "\n"
              "    if io.write_line(\"review-audit-stable\") != 0 {\n"
              "        return 93\n"
              "    }\n"
              "    return 0\n"
              "}\n"
              "\n"
              "func run_focus(path: str) i32 {\n"
              "    text_buf: *Buffer<u8> = load_review_text(path)\n"
              "    if text_buf == nil {\n"
              "        return 94\n"
              "    }\n"
              "    defer mem.buffer_free<u8>(text_buf)\n"
              "\n"
              "    if focus_needs_escalation_text(buffer_text(text_buf)) {\n"
              "        if io.write_line(\"review-focus-escalate\") != 0 {\n"
              "            return 95\n"
              "        }\n"
              "        return 0\n"
              "    }\n"
              "\n"
              "    if io.write_line(\"review-focus-normal\") != 0 {\n"
              "        return 96\n"
              "    }\n"
              "    return 0\n"
              "}\n");

    BuildProjectTargetAndExpectSuccess(mc_path,
                                       cloned_project_path,
                                       rebuild_build_dir,
                                       "",
                                       "review_board_interface_audit_build.txt",
                                       "review board interface-changing audit rebuild");

    const auto review_status_object_time_3 = RequireWriteTime(review_status_object);
    const auto audit_main_object_time_3 = RequireWriteTime(audit_main_object);
    const auto focus_main_object_time_3 = RequireWriteTime(focus_main_object);
    const auto review_status_mci_time_3 = RequireWriteTime(review_status_mci);
    const auto audit_executable_time_3 = RequireWriteTime(audit_executable);

    if (RequireWriteTime(review_scan_object) != review_scan_object_time_2) {
        Fail("phase22 interface-changing status edit should reuse the deep shared scan object");
    }
    if (!(review_status_object_time_3 > review_status_object_time_1)) {
        Fail("phase22 interface-changing status edit should rebuild the direct shared status object");
    }
    if (!(audit_main_object_time_3 > audit_main_object_time_1)) {
        Fail("phase22 interface-changing status edit should rebuild the default-target main object");
    }
    if (focus_main_object_time_3 != focus_main_object_time_1) {
        Fail("phase22 interface-changing audit rebuild should defer the explicit-target main rebuild until that target is selected");
    }
    if (!(review_status_mci_time_3 > review_status_mci_time_1)) {
        Fail("phase22 interface-changing status edit should rewrite the shared status interface artifact");
    }
    if (!(audit_executable_time_3 > audit_executable_time_2)) {
        Fail("phase22 interface-changing status edit should relink the default-target executable");
    }
    if (RequireWriteTime(focus_executable) != focus_executable_time_2) {
        Fail("phase22 interface-changing audit rebuild should not touch the explicit-target executable before it is selected");
    }

    SleepForTimestampTick();
    BuildProjectTargetAndExpectSuccess(mc_path,
                                       cloned_project_path,
                                       rebuild_build_dir,
                                       "focus",
                                       "review_board_interface_focus_build.txt",
                                       "review board interface-changing focus rebuild");

    if (RequireWriteTime(review_status_object) != review_status_object_time_3) {
        Fail("phase22 interface-changing focus rebuild should reuse the already rebuilt direct shared status object");
    }
    if (!(RequireWriteTime(focus_main_object) > focus_main_object_time_1)) {
        Fail("phase22 interface-changing focus rebuild should rebuild the explicit-target main object");
    }
    if (!(RequireWriteTime(focus_executable) > focus_executable_time_2)) {
        Fail("phase22 interface-changing focus rebuild should relink the explicit-target executable");
    }
}

void TestRealIssueRollupProject(const std::filesystem::path& source_root,
                                const std::filesystem::path& binary_root,
                                const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = source_root / "examples/real/issue_rollup";
    const std::filesystem::path project_path = project_root / "build.toml";
    const std::filesystem::path sample_path = project_root / "tests/sample.txt";

    const std::filesystem::path core_build_dir = binary_root / "issue_rollup_core_build";
    std::filesystem::remove_all(core_build_dir);
    const std::string core_build_output = BuildProjectTargetAndCapture(mc_path,
                                                                       project_path,
                                                                       core_build_dir,
                                                                       "issue-rollup-core",
                                                                       "issue_rollup_core_build_output.txt",
                                                                       "issue rollup explicit static library build");
    const auto core_archive = mc::support::ComputeBuildArtifactTargets(project_root / "src/core/rollup_core.mc",
                                                                       core_build_dir)
                                  .static_library;
    if (!std::filesystem::exists(core_archive)) {
        Fail("phase29 explicit static library build should emit the deterministic archive artifact");
    }
    ExpectOutputContains(core_build_output,
                         "built target issue-rollup-core -> " + core_archive.generic_string(),
                         "phase29 explicit static library build should report the archive path");

    const std::string staticlib_run_output = RunProjectTargetAndExpectFailure(mc_path,
                                                                              project_path,
                                                                              core_build_dir,
                                                                              "issue-rollup-core",
                                                                              "issue_rollup_core_run_output.txt",
                                                                              "issue rollup static library run rejection");
    ExpectOutputContains(staticlib_run_output,
                         "target 'issue-rollup-core' has kind 'staticlib' and cannot be run; choose an executable target or use mc build",
                         "phase29 static library targets should be rejected by mc run");

    const std::filesystem::path run_test_rerun_build_dir = binary_root / "issue_rollup_run_test_rerun_build";
    std::filesystem::remove_all(run_test_rerun_build_dir);

    std::string run_output = RunProjectTargetAndExpectSuccess(mc_path,
                                                              project_path,
                                                              run_test_rerun_build_dir,
                                                              "",
                                                              sample_path,
                                                              "issue_rollup_run_output.txt",
                                                              "issue rollup run before tests");
    ExpectIssueRollupRunOutput(run_output,
                               "issue-rollup-steady\n",
                               "phase29 issue rollup run before tests");

    std::string test_output = RunProjectTestAndExpectSuccess(mc_path,
                                                             project_path,
                                                             run_test_rerun_build_dir,
                                                             "issue_rollup_test_output.txt",
                                                             "issue rollup test after run");
    ExpectIssueRollupTestOutput(test_output,
                                "phase29 issue rollup test after run");

    run_output = RunProjectTargetAndExpectSuccess(mc_path,
                                                  project_path,
                                                  run_test_rerun_build_dir,
                                                  "",
                                                  sample_path,
                                                  "issue_rollup_rerun_output.txt",
                                                  "issue rollup rerun after tests");
    ExpectIssueRollupRunOutput(run_output,
                               "issue-rollup-steady\n",
                               "phase29 issue rollup rerun after tests");

    const std::filesystem::path test_run_rerun_build_dir = binary_root / "issue_rollup_test_run_rerun_build";
    std::filesystem::remove_all(test_run_rerun_build_dir);

    test_output = RunProjectTestAndExpectSuccess(mc_path,
                                                 project_path,
                                                 test_run_rerun_build_dir,
                                                 "issue_rollup_initial_test_output.txt",
                                                 "issue rollup initial test");
    ExpectIssueRollupTestOutput(test_output,
                                "phase29 issue rollup initial test");

    run_output = RunProjectTargetAndExpectSuccess(mc_path,
                                                  project_path,
                                                  test_run_rerun_build_dir,
                                                  "",
                                                  sample_path,
                                                  "issue_rollup_run_after_test_output.txt",
                                                  "issue rollup run after tests");
    ExpectIssueRollupRunOutput(run_output,
                               "issue-rollup-steady\n",
                               "phase29 issue rollup run after tests");

    test_output = RunProjectTestAndExpectSuccess(mc_path,
                                                 project_path,
                                                 test_run_rerun_build_dir,
                                                 "issue_rollup_retest_output.txt",
                                                 "issue rollup retest after run");
    ExpectIssueRollupTestOutput(test_output,
                                "phase29 issue rollup retest after run");

    {
        const std::filesystem::path selected_target_reuse_build_dir = binary_root / "issue_rollup_selected_target_reuse_build";
        std::filesystem::remove_all(selected_target_reuse_build_dir);

        BuildProjectTargetAndExpectSuccess(mc_path,
                                           project_path,
                                           selected_target_reuse_build_dir,
                                           "",
                                           "issue_rollup_initial_build.txt",
                                           "issue rollup initial default-target build");

        const auto rollup_model_object = mc::support::ComputeBuildArtifactTargets(project_root / "src/model/rollup_model.mc",
                                                                                  selected_target_reuse_build_dir)
                                             .object;
        const auto rollup_parse_object = mc::support::ComputeBuildArtifactTargets(project_root / "src/parse/rollup_parse.mc",
                                                                                  selected_target_reuse_build_dir)
                                             .object;
        const auto rollup_render_object = mc::support::ComputeBuildArtifactTargets(project_root / "src/render/rollup_render.mc",
                                                                                   selected_target_reuse_build_dir)
                                              .object;
        const auto rollup_core_object = mc::support::ComputeBuildArtifactTargets(project_root / "src/core/rollup_core.mc",
                                                                                 selected_target_reuse_build_dir)
                                             .object;
        const auto app_main_object = mc::support::ComputeBuildArtifactTargets(project_root / "src/app/main.mc",
                                                                              selected_target_reuse_build_dir)
                                         .object;
        const auto report_main_object = mc::support::ComputeBuildArtifactTargets(project_root / "src/app/report_main.mc",
                                                                                 selected_target_reuse_build_dir)
                                            .object;
        const auto issue_rollup_core_archive = mc::support::ComputeBuildArtifactTargets(project_root / "src/core/rollup_core.mc",
                                                                                         selected_target_reuse_build_dir)
                                             .static_library;
        const auto issue_rollup_executable = mc::support::ComputeBuildArtifactTargets(project_root / "src/app/main.mc",
                                                                                      selected_target_reuse_build_dir)
                                                 .executable;
        const auto issue_rollup_report_executable = mc::support::ComputeBuildArtifactTargets(project_root / "src/app/report_main.mc",
                                                                                             selected_target_reuse_build_dir)
                                                     .executable;

        const auto rollup_model_object_time_1 = RequireWriteTime(rollup_model_object);
        const auto rollup_parse_object_time_1 = RequireWriteTime(rollup_parse_object);
        const auto rollup_render_object_time_1 = RequireWriteTime(rollup_render_object);
        const auto rollup_core_object_time_1 = RequireWriteTime(rollup_core_object);
        const auto app_main_object_time_1 = RequireWriteTime(app_main_object);
        const auto issue_rollup_core_archive_time_1 = RequireWriteTime(issue_rollup_core_archive);
        const auto issue_rollup_executable_time_1 = RequireWriteTime(issue_rollup_executable);
        if (std::filesystem::exists(report_main_object) || std::filesystem::exists(issue_rollup_report_executable)) {
            Fail("phase74 initial default-target build should not emit report-target artifacts");
        }

        SleepForTimestampTick();
        BuildProjectTargetAndExpectSuccess(mc_path,
                                           project_path,
                                           selected_target_reuse_build_dir,
                                           "issue-rollup-report",
                                           "issue_rollup_report_build.txt",
                                           "issue rollup explicit report-target build");

        const auto report_main_object_time_1 = RequireWriteTime(report_main_object);
        const auto issue_rollup_report_executable_time_1 = RequireWriteTime(issue_rollup_report_executable);
        if (RequireWriteTime(rollup_model_object) != rollup_model_object_time_1 ||
            RequireWriteTime(rollup_parse_object) != rollup_parse_object_time_1 ||
            RequireWriteTime(rollup_render_object) != rollup_render_object_time_1 ||
            RequireWriteTime(rollup_core_object) != rollup_core_object_time_1 ||
            RequireWriteTime(app_main_object) != app_main_object_time_1 ||
            RequireWriteTime(issue_rollup_core_archive) != issue_rollup_core_archive_time_1 ||
            RequireWriteTime(issue_rollup_executable) != issue_rollup_executable_time_1) {
            Fail("phase74 explicit report-target build should reuse shared objects, archive output, and the default executable artifacts");
        }

        std::string report_run_output = RunProjectTargetAndExpectSuccess(mc_path,
                                                                         project_path,
                                                                         selected_target_reuse_build_dir,
                                                                         "issue-rollup-report",
                                                                         sample_path,
                                                                         "issue_rollup_report_run.txt",
                                                                         "issue rollup explicit report-target run");
        ExpectIssueRollupRunOutput(report_run_output,
                                   "issue-rollup-steady\n",
                                   "phase74 issue rollup explicit report-target run");
        if (RequireWriteTime(rollup_model_object) != rollup_model_object_time_1 ||
            RequireWriteTime(rollup_parse_object) != rollup_parse_object_time_1 ||
            RequireWriteTime(rollup_render_object) != rollup_render_object_time_1 ||
            RequireWriteTime(rollup_core_object) != rollup_core_object_time_1 ||
            RequireWriteTime(app_main_object) != app_main_object_time_1 ||
            RequireWriteTime(report_main_object) != report_main_object_time_1 ||
            RequireWriteTime(issue_rollup_core_archive) != issue_rollup_core_archive_time_1 ||
            RequireWriteTime(issue_rollup_executable) != issue_rollup_executable_time_1 ||
            RequireWriteTime(issue_rollup_report_executable) != issue_rollup_report_executable_time_1) {
            Fail("phase74 explicit report-target run should reuse all already built artifacts without touching the sibling executable");
        }

        SleepForTimestampTick();
        BuildProjectTargetAndExpectSuccess(mc_path,
                                           project_path,
                                           selected_target_reuse_build_dir,
                                           "",
                                           "issue_rollup_default_rebuild.txt",
                                           "issue rollup default-target no-op rebuild");

        if (RequireWriteTime(rollup_model_object) != rollup_model_object_time_1 ||
            RequireWriteTime(rollup_parse_object) != rollup_parse_object_time_1 ||
            RequireWriteTime(rollup_render_object) != rollup_render_object_time_1 ||
            RequireWriteTime(rollup_core_object) != rollup_core_object_time_1 ||
            RequireWriteTime(app_main_object) != app_main_object_time_1 ||
            RequireWriteTime(report_main_object) != report_main_object_time_1 ||
            RequireWriteTime(issue_rollup_core_archive) != issue_rollup_core_archive_time_1 ||
            RequireWriteTime(issue_rollup_executable) != issue_rollup_executable_time_1 ||
            RequireWriteTime(issue_rollup_report_executable) != issue_rollup_report_executable_time_1) {
            Fail("phase74 default-target no-op rebuild should preserve shared artifacts and the non-selected report executable");
        }
    }

    const std::filesystem::path cloned_project_root = binary_root / "issue_rollup_clone";
    CopyDirectoryTree(project_root, cloned_project_root);
    WriteFile(cloned_project_root / "build.toml",
              "schema = 1\n"
              "project = \"issue-rollup\"\n"
              "default = \"issue-rollup\"\n"
              "\n"
              "[targets.issue-rollup-core]\n"
              "kind = \"staticlib\"\n"
              "package = \"issue-rollup\"\n"
              "root = \"src/core/rollup_core.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"hosted\"\n"
              "\n"
              "[targets.issue-rollup-core.search_paths]\n"
              "modules = [\"src/core\", \"src/model\", \"src/parse\", \"src/render\", \"" +
                  (source_root / "stdlib").generic_string() + "\"]\n"
              "\n"
              "[targets.issue-rollup-core.runtime]\n"
              "startup = \"default\"\n"
              "\n"
              "[targets.issue-rollup]\n"
              "kind = \"exe\"\n"
              "package = \"issue-rollup\"\n"
              "root = \"src/app/main.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"hosted\"\n"
              "links = [\"issue-rollup-core\"]\n"
              "\n"
              "[targets.issue-rollup.search_paths]\n"
              "modules = [\"src/app\", \"src/core\", \"src/model\", \"src/parse\", \"src/render\", \"" +
                  (source_root / "stdlib").generic_string() + "\"]\n"
              "\n"
              "[targets.issue-rollup.runtime]\n"
              "startup = \"default\"\n"
              "\n"
              "[targets.issue-rollup-report]\n"
              "kind = \"exe\"\n"
              "package = \"issue-rollup\"\n"
              "root = \"src/app/report_main.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"hosted\"\n"
              "links = [\"issue-rollup-core\"]\n"
              "\n"
              "[targets.issue-rollup-report.search_paths]\n"
              "modules = [\"src/app\", \"src/core\", \"src/model\", \"src/parse\", \"src/render\", \"" +
                  (source_root / "stdlib").generic_string() + "\"]\n"
              "\n"
              "[targets.issue-rollup-report.runtime]\n"
              "startup = \"default\"\n"
              "\n"
              "[targets.issue-rollup.tests]\n"
              "enabled = true\n"
              "roots = [\"tests\"]\n"
              "mode = \"checked\"\n"
              "timeout_ms = 5000\n");
    WriteFile(cloned_project_root / "src/app/report_main.mc",
              "import fs\n"
              "import mem\n"
              "import rollup_core\n"
              "\n"
              "func main(args: Slice<cstr>) i32 {\n"
              "    if args.len != 2 {\n"
              "        return 64\n"
              "    }\n"
              "\n"
              "    buf: *Buffer<u8> = fs.read_all(args[1], mem.default_allocator())\n"
              "    if buf == nil {\n"
              "        return 92\n"
              "    }\n"
              "    defer mem.buffer_free<u8>(buf)\n"
              "\n"
              "    bytes: Slice<u8> = mem.slice_from_buffer<u8>(buf)\n"
              "    text: str = str{ ptr: bytes.ptr, len: bytes.len }\n"
              "    return rollup_core.write_text_rollup(text)\n"
              "}\n");
    const std::filesystem::path cloned_project_path = cloned_project_root / "build.toml";
    const std::filesystem::path cloned_sample_path = cloned_project_root / "tests/sample.txt";
    const std::filesystem::path rebuild_build_dir = binary_root / "issue_rollup_rebuild_build";
    std::filesystem::remove_all(rebuild_build_dir);

    BuildProjectTargetAndExpectSuccess(mc_path,
                                       cloned_project_path,
                                       rebuild_build_dir,
                                       "",
                                       "issue_rollup_initial_build.txt",
                                       "issue rollup initial build");
    BuildProjectTargetAndExpectSuccess(mc_path,
                                       cloned_project_path,
                                       rebuild_build_dir,
                                       "issue-rollup-report",
                                       "issue_rollup_initial_report_build.txt",
                                       "issue rollup initial report-target build");

    const auto rollup_model_object = mc::support::ComputeBuildArtifactTargets(cloned_project_root / "src/model/rollup_model.mc",
                                                                              rebuild_build_dir)
                                         .object;
    const auto rollup_parse_object = mc::support::ComputeBuildArtifactTargets(cloned_project_root / "src/parse/rollup_parse.mc",
                                                                              rebuild_build_dir)
                                         .object;
    const auto rollup_render_object = mc::support::ComputeBuildArtifactTargets(cloned_project_root / "src/render/rollup_render.mc",
                                                                               rebuild_build_dir)
                                          .object;
    const auto rollup_core_object = mc::support::ComputeBuildArtifactTargets(cloned_project_root / "src/core/rollup_core.mc",
                                                                             rebuild_build_dir)
                                         .object;
    const auto app_main_object = mc::support::ComputeBuildArtifactTargets(cloned_project_root / "src/app/main.mc",
                                                                          rebuild_build_dir)
                                     .object;
    const auto report_main_object = mc::support::ComputeBuildArtifactTargets(cloned_project_root / "src/app/report_main.mc",
                                                                             rebuild_build_dir)
                                        .object;
    const auto rollup_model_mci = mc::support::ComputeDumpTargets(cloned_project_root / "src/model/rollup_model.mc",
                                                                  rebuild_build_dir)
                                      .mci;
    const auto rollup_parse_mci = mc::support::ComputeDumpTargets(cloned_project_root / "src/parse/rollup_parse.mc",
                                                                  rebuild_build_dir)
                                      .mci;
    const auto rollup_render_mci = mc::support::ComputeDumpTargets(cloned_project_root / "src/render/rollup_render.mc",
                                                                   rebuild_build_dir)
                                       .mci;
    const auto rollup_core_mci = mc::support::ComputeDumpTargets(cloned_project_root / "src/core/rollup_core.mc",
                                                                 rebuild_build_dir)
                                     .mci;
    const auto issue_rollup_core_archive = mc::support::ComputeBuildArtifactTargets(cloned_project_root / "src/core/rollup_core.mc",
                                                                                     rebuild_build_dir)
                                         .static_library;
    const auto issue_rollup_executable = mc::support::ComputeBuildArtifactTargets(cloned_project_root / "src/app/main.mc",
                                                                                  rebuild_build_dir)
                                             .executable;
    const auto issue_rollup_report_executable = mc::support::ComputeBuildArtifactTargets(cloned_project_root / "src/app/report_main.mc",
                                                                                         rebuild_build_dir)
                                                 .executable;

    const auto rollup_model_object_time_1 = RequireWriteTime(rollup_model_object);
    const auto rollup_parse_object_time_1 = RequireWriteTime(rollup_parse_object);
    const auto rollup_render_object_time_1 = RequireWriteTime(rollup_render_object);
    const auto rollup_core_object_time_1 = RequireWriteTime(rollup_core_object);
    const auto app_main_object_time_1 = RequireWriteTime(app_main_object);
    const auto report_main_object_time_1 = RequireWriteTime(report_main_object);
    const auto rollup_model_mci_time_1 = RequireWriteTime(rollup_model_mci);
    const auto rollup_parse_mci_time_1 = RequireWriteTime(rollup_parse_mci);
    const auto rollup_render_mci_time_1 = RequireWriteTime(rollup_render_mci);
    const auto rollup_core_mci_time_1 = RequireWriteTime(rollup_core_mci);
    const std::string rollup_model_mci_text_1 = ReadFile(rollup_model_mci);
    const std::string rollup_parse_mci_text_1 = ReadFile(rollup_parse_mci);
    const std::string rollup_render_mci_text_1 = ReadFile(rollup_render_mci);
    const std::string rollup_core_mci_text_1 = ReadFile(rollup_core_mci);
    const auto issue_rollup_core_archive_time_1 = RequireWriteTime(issue_rollup_core_archive);
    const auto issue_rollup_executable_time_1 = RequireWriteTime(issue_rollup_executable);
    const auto issue_rollup_report_executable_time_1 = RequireWriteTime(issue_rollup_report_executable);

    const auto [report_initial_outcome, report_initial_output] = RunCommandCapture({issue_rollup_report_executable.generic_string(),
                                                                                    cloned_sample_path.generic_string()},
                                                                                   rebuild_build_dir /
                                                                                           "issue_rollup_initial_report_run.txt",
                                                                                       "issue rollup initial report-target executable run");
    if (!report_initial_outcome.exited || report_initial_outcome.exit_code != 0) {
        Fail("phase30 initial report-target executable run should pass:\n" + report_initial_output);
    }
    ExpectIssueRollupRunOutput(report_initial_output,
                               "issue-rollup-steady\n",
                               "phase30 issue rollup initial report-target executable run");

    SleepForTimestampTick();
    BuildProjectTargetAndExpectSuccess(mc_path,
                                       cloned_project_path,
                                       rebuild_build_dir,
                                       "",
                                       "issue_rollup_noop_build.txt",
                                       "issue rollup no-op build");

    if (RequireWriteTime(rollup_model_object) != rollup_model_object_time_1 ||
        RequireWriteTime(rollup_parse_object) != rollup_parse_object_time_1 ||
        RequireWriteTime(rollup_render_object) != rollup_render_object_time_1 ||
        RequireWriteTime(rollup_core_object) != rollup_core_object_time_1 ||
        RequireWriteTime(app_main_object) != app_main_object_time_1 ||
        RequireWriteTime(report_main_object) != report_main_object_time_1 ||
        RequireWriteTime(issue_rollup_core_archive) != issue_rollup_core_archive_time_1 ||
        RequireWriteTime(issue_rollup_executable) != issue_rollup_executable_time_1 ||
        RequireWriteTime(issue_rollup_report_executable) != issue_rollup_report_executable_time_1) {
        Fail("phase29 no-op build should reuse grouped-package objects, archive output, and both executable outputs");
    }
    if (ReadFile(rollup_model_mci) != rollup_model_mci_text_1 ||
        ReadFile(rollup_parse_mci) != rollup_parse_mci_text_1 ||
        ReadFile(rollup_render_mci) != rollup_render_mci_text_1 ||
        ReadFile(rollup_core_mci) != rollup_core_mci_text_1) {
        Fail("phase29 no-op build should preserve grouped-package interface artifact contents");
    }

    SleepForTimestampTick();
    WriteFile(cloned_project_root / "src/parse/rollup_parse.mc",
              "import rollup_model\n"
              "import strings\n"
              "\n"
              "func line_kind(bytes: Slice<u8>, start: usize, newline: usize) u8 {\n"
              "    if newline <= start {\n"
              "        return 0\n"
              "    }\n"
              "    return bytes[start]\n"
              "}\n"
              "\n"
              "func line_is_priority(bytes: Slice<u8>, start: usize, newline: usize) bool {\n"
              "    kind: u8 = line_kind(bytes, start, newline)\n"
              "    if kind == 66 {\n"
              "        return true\n"
              "    }\n"
              "    if newline <= start + 1 {\n"
              "        return false\n"
              "    }\n"
              "    return bytes[start + 1] == 33\n"
              "}\n"
              "\n"
              "func summarize_text(text: str) rollup_model.Summary {\n"
              "    bytes: Slice<u8> = strings.bytes(text)\n"
              "    open_items: usize = 0\n"
              "    closed_items: usize = 0\n"
              "    blocked_items: usize = 0\n"
              "    priority_items: usize = 0\n"
              "    start: usize = 0\n"
              "    while start < text.len {\n"
              "        newline: usize = start\n"
              "        while newline < text.len {\n"
              "            if bytes[newline] == 10 {\n"
              "                break\n"
              "            }\n"
              "            newline = newline + 1\n"
              "        }\n"
              "\n"
              "        kind: u8 = line_kind(bytes, start, newline)\n"
              "        if kind == 79 {\n"
              "            open_items = open_items + 1\n"
              "        }\n"
              "        if kind == 67 {\n"
              "            closed_items = closed_items + 1\n"
              "        }\n"
              "        if kind == 66 {\n"
              "            blocked_items = blocked_items + 1\n"
              "        }\n"
              "        if line_is_priority(bytes, start, newline) {\n"
              "            priority_items = priority_items + 1\n"
              "        }\n"
              "\n"
              "        if newline == text.len {\n"
              "            break\n"
              "        }\n"
              "        start = newline + 1\n"
              "    }\n"
              "\n"
              "    return rollup_model.Summary{ open_items: open_items, closed_items: closed_items, blocked_items: blocked_items, priority_items: priority_items }\n"
              "}\n");

    BuildProjectTargetAndExpectSuccess(mc_path,
                                       cloned_project_path,
                                       rebuild_build_dir,
                                       "",
                                       "issue_rollup_impl_build.txt",
                                       "issue rollup implementation-only parse rebuild");

    const auto rollup_model_object_time_2 = RequireWriteTime(rollup_model_object);
    const auto rollup_parse_object_time_2 = RequireWriteTime(rollup_parse_object);
    const auto rollup_render_object_time_2 = RequireWriteTime(rollup_render_object);
    const auto rollup_core_object_time_2 = RequireWriteTime(rollup_core_object);
    const auto app_main_object_time_2 = RequireWriteTime(app_main_object);
    const auto report_main_object_time_2 = RequireWriteTime(report_main_object);
    const auto rollup_model_mci_time_2 = RequireWriteTime(rollup_model_mci);
    const auto rollup_parse_mci_time_2 = RequireWriteTime(rollup_parse_mci);
    const auto rollup_render_mci_time_2 = RequireWriteTime(rollup_render_mci);
    const auto rollup_core_mci_time_2 = RequireWriteTime(rollup_core_mci);
    const std::string rollup_model_mci_text_2 = ReadFile(rollup_model_mci);
    const std::string rollup_parse_mci_text_2 = ReadFile(rollup_parse_mci);
    const std::string rollup_render_mci_text_2 = ReadFile(rollup_render_mci);
    const std::string rollup_core_mci_text_2 = ReadFile(rollup_core_mci);
    const auto issue_rollup_core_archive_time_2 = RequireWriteTime(issue_rollup_core_archive);
    const auto issue_rollup_executable_time_2 = RequireWriteTime(issue_rollup_executable);
    const auto issue_rollup_report_executable_time_2 = RequireWriteTime(issue_rollup_report_executable);

    if (rollup_model_object_time_2 != rollup_model_object_time_1) {
        Fail("phase29 implementation-only parse edit should not rebuild the model package object");
    }
    if (!(rollup_parse_object_time_2 > rollup_parse_object_time_1)) {
        Fail("phase29 implementation-only parse edit should rebuild the parse package object");
    }
    if (rollup_core_object_time_2 != rollup_core_object_time_1) {
        Fail("phase29 implementation-only parse edit should not rebuild the static-library entry object");
    }
    if (app_main_object_time_2 != app_main_object_time_1) {
        Fail("phase29 implementation-only parse edit should not rebuild the executable root object");
    }
    if (report_main_object_time_2 != report_main_object_time_1) {
        Fail("phase30 implementation-only parse edit should not rebuild the non-selected report-target root object");
    }
    if (rollup_model_mci_text_2 != rollup_model_mci_text_1) {
        Fail("phase29 implementation-only parse edit should preserve the model interface artifact contents");
    }
    if (rollup_parse_mci_text_2 != rollup_parse_mci_text_1) {
        Fail("phase29 implementation-only parse edit should preserve the parse interface artifact contents");
    }
    if (rollup_render_mci_text_2 != rollup_render_mci_text_1) {
        Fail("phase29 implementation-only parse edit should preserve the render interface artifact contents");
    }
    if (rollup_core_mci_text_2 != rollup_core_mci_text_1) {
        Fail("phase29 implementation-only parse edit should preserve the static-library entry interface artifact contents");
    }
    if (!(issue_rollup_core_archive_time_2 > issue_rollup_core_archive_time_1)) {
        Fail("phase29 implementation-only parse edit should rebuild the static library archive");
    }
    if (!(issue_rollup_executable_time_2 > issue_rollup_executable_time_1)) {
        Fail("phase29 implementation-only parse edit should relink the executable");
    }
    if (issue_rollup_report_executable_time_2 != issue_rollup_report_executable_time_1) {
        Fail("phase30 implementation-only default-target rebuild should not touch the non-selected report executable");
    }

    const auto [impl_outcome, impl_output] = RunCommandCapture({issue_rollup_executable.generic_string(),
                                                                cloned_sample_path.generic_string()},
                                                               rebuild_build_dir / "issue_rollup_impl_run.txt",
                                                               "issue rollup implementation-only executable run");
    if (!impl_outcome.exited || impl_outcome.exit_code != 0) {
        Fail("phase29 implementation-only executable run should pass:\n" + impl_output);
    }
    ExpectIssueRollupRunOutput(impl_output,
                               "issue-rollup-attention\n",
                               "phase29 issue rollup implementation-only executable run");

    SleepForTimestampTick();
    BuildProjectTargetAndExpectSuccess(mc_path,
                                       cloned_project_path,
                                       rebuild_build_dir,
                                       "issue-rollup-report",
                                       "issue_rollup_impl_report_build.txt",
                                       "issue rollup implementation-only report-target rebuild");

    const auto report_main_object_time_2_selected = RequireWriteTime(report_main_object);
    const auto issue_rollup_report_executable_time_2_selected = RequireWriteTime(issue_rollup_report_executable);
    if (RequireWriteTime(rollup_model_object) != rollup_model_object_time_2) {
        Fail("phase30 implementation-only report-target rebuild should reuse the already rebuilt model object");
    }
    if (RequireWriteTime(rollup_parse_object) != rollup_parse_object_time_2) {
        Fail("phase30 implementation-only report-target rebuild should reuse the already rebuilt parse object");
    }
    if (RequireWriteTime(rollup_render_object) != rollup_render_object_time_2) {
        Fail("phase30 implementation-only report-target rebuild should reuse the render object");
    }
    if (RequireWriteTime(rollup_core_object) != rollup_core_object_time_2) {
        Fail("phase30 implementation-only report-target rebuild should reuse the static-library entry object");
    }
    if (report_main_object_time_2_selected != report_main_object_time_2) {
        Fail("phase30 implementation-only report-target rebuild should reuse the report-target root object");
    }
    if (!(issue_rollup_report_executable_time_2_selected > issue_rollup_report_executable_time_2)) {
        Fail("phase30 implementation-only report-target rebuild should relink the selected report executable");
    }

    const auto [impl_report_outcome, impl_report_output] = RunCommandCapture({issue_rollup_report_executable.generic_string(),
                                                                              cloned_sample_path.generic_string()},
                                                                             rebuild_build_dir /
                                                                                 "issue_rollup_impl_report_run.txt",
                                                                             "issue rollup implementation-only report-target executable run");
    if (!impl_report_outcome.exited || impl_report_outcome.exit_code != 0) {
        Fail("phase30 implementation-only report-target executable run should pass:\n" + impl_report_output);
    }
    ExpectIssueRollupRunOutput(impl_report_output,
                               "issue-rollup-attention\n",
                               "phase30 issue rollup implementation-only report-target executable run");

    SleepForTimestampTick();
    WriteFile(cloned_project_root / "src/core/rollup_core.mc",
              "import rollup_model\n"
              "import rollup_parse\n"
              "import rollup_render\n"
              "\n"
              "func helper_version() i32 {\n"
              "    return 29\n"
              "}\n"
              "\n"
              "func summarize_text(text: str) rollup_model.Summary {\n"
              "    return rollup_parse.summarize_text(text)\n"
              "}\n"
              "\n"
              "func write_text_rollup(text: str) i32 {\n"
              "    summary: rollup_model.Summary = summarize_text(text)\n"
              "    return rollup_render.write_rollup(summary)\n"
              "}\n");

    BuildProjectTargetAndExpectSuccess(mc_path,
                                       cloned_project_path,
                                       rebuild_build_dir,
                                       "",
                                       "issue_rollup_interface_build.txt",
                                       "issue rollup interface-changing core rebuild");

    const auto rollup_model_object_time_3 = RequireWriteTime(rollup_model_object);
    const auto rollup_parse_object_time_3 = RequireWriteTime(rollup_parse_object);
    const auto rollup_render_object_time_3 = RequireWriteTime(rollup_render_object);
    const auto rollup_core_object_time_3 = RequireWriteTime(rollup_core_object);
    const auto app_main_object_time_3 = RequireWriteTime(app_main_object);
    const auto report_main_object_time_3 = RequireWriteTime(report_main_object);
    const auto rollup_model_mci_time_3 = RequireWriteTime(rollup_model_mci);
    const auto rollup_parse_mci_time_3 = RequireWriteTime(rollup_parse_mci);
    const auto rollup_render_mci_time_3 = RequireWriteTime(rollup_render_mci);
    const auto rollup_core_mci_time_3 = RequireWriteTime(rollup_core_mci);
    const auto issue_rollup_core_archive_time_3 = RequireWriteTime(issue_rollup_core_archive);
    const auto issue_rollup_executable_time_3 = RequireWriteTime(issue_rollup_executable);
    const auto issue_rollup_report_executable_time_3 = RequireWriteTime(issue_rollup_report_executable);

    if (rollup_model_object_time_3 != rollup_model_object_time_2) {
        Fail("phase29 interface-changing core edit should not rebuild the model package object");
    }
    if (rollup_parse_object_time_3 != rollup_parse_object_time_2) {
        Fail("phase29 interface-changing core edit should not rebuild the parse package object");
    }
    if (!(rollup_core_object_time_3 > rollup_core_object_time_2)) {
        Fail("phase29 interface-changing core edit should rebuild the static-library entry object");
    }
    if (!(app_main_object_time_3 > app_main_object_time_2)) {
        Fail("phase29 interface-changing core edit should rebuild the executable root object");
    }
    if (report_main_object_time_3 != report_main_object_time_2_selected) {
        Fail("phase30 interface-changing default-target rebuild should not touch the non-selected report-target root object");
    }
    if (ReadFile(rollup_model_mci) != rollup_model_mci_text_2) {
        Fail("phase29 interface-changing core edit should preserve the model interface artifact contents");
    }
    if (ReadFile(rollup_parse_mci) != rollup_parse_mci_text_2) {
        Fail("phase29 interface-changing core edit should preserve the parse interface artifact contents");
    }
    if (ReadFile(rollup_render_mci) != rollup_render_mci_text_2) {
        Fail("phase29 interface-changing core edit should preserve the render interface artifact contents");
    }
    if (!(rollup_core_mci_time_3 > rollup_core_mci_time_2)) {
        Fail("phase29 interface-changing core edit should rewrite the static-library entry interface artifact");
    }
    if (!(issue_rollup_core_archive_time_3 > issue_rollup_core_archive_time_2)) {
        Fail("phase29 interface-changing core edit should rebuild the static library archive");
    }
    if (!(issue_rollup_executable_time_3 > issue_rollup_executable_time_2)) {
        Fail("phase29 interface-changing core edit should relink the executable");
    }
    if (issue_rollup_report_executable_time_3 != issue_rollup_report_executable_time_2_selected) {
        Fail("phase30 interface-changing default-target rebuild should not touch the non-selected report executable");
    }

    const auto [interface_outcome, interface_output] = RunCommandCapture({issue_rollup_executable.generic_string(),
                                                                          cloned_sample_path.generic_string()},
                                                                         rebuild_build_dir /
                                                                             "issue_rollup_interface_run.txt",
                                                                         "issue rollup interface-changing executable run");
    if (!interface_outcome.exited || interface_outcome.exit_code != 0) {
        Fail("phase29 interface-changing executable run should pass:\n" + interface_output);
    }
    ExpectIssueRollupRunOutput(interface_output,
                               "issue-rollup-attention\n",
                               "phase29 issue rollup interface-changing executable run");

    SleepForTimestampTick();
    BuildProjectTargetAndExpectSuccess(mc_path,
                                       cloned_project_path,
                                       rebuild_build_dir,
                                       "issue-rollup-report",
                                       "issue_rollup_interface_report_build.txt",
                                       "issue rollup interface-changing report-target rebuild");

    const auto report_main_object_time_4 = RequireWriteTime(report_main_object);
    const auto issue_rollup_report_executable_time_4 = RequireWriteTime(issue_rollup_report_executable);
    if (RequireWriteTime(rollup_model_object) != rollup_model_object_time_3) {
        Fail("phase30 interface-changing report-target rebuild should reuse the model object");
    }
    if (RequireWriteTime(rollup_parse_object) != rollup_parse_object_time_3) {
        Fail("phase30 interface-changing report-target rebuild should reuse the parse object");
    }
    if (RequireWriteTime(rollup_render_object) != rollup_render_object_time_3) {
        Fail("phase30 interface-changing report-target rebuild should reuse the render object");
    }
    if (RequireWriteTime(rollup_core_object) != rollup_core_object_time_3) {
        Fail("phase30 interface-changing report-target rebuild should reuse the already rebuilt static-library entry object");
    }
    if (!(report_main_object_time_4 > report_main_object_time_3)) {
        Fail("phase30 interface-changing report-target rebuild should rebuild the selected report-target root object");
    }
    if (!(issue_rollup_report_executable_time_4 > issue_rollup_report_executable_time_3)) {
        Fail("phase30 interface-changing report-target rebuild should relink the selected report executable");
    }

    const auto [interface_report_outcome, interface_report_output] = RunCommandCapture({issue_rollup_report_executable.generic_string(),
                                                                                        cloned_sample_path.generic_string()},
                                                                                       rebuild_build_dir /
                                                                                           "issue_rollup_interface_report_run.txt",
                                                                                       "issue rollup interface-changing report-target executable run");
    if (!interface_report_outcome.exited || interface_report_outcome.exit_code != 0) {
        Fail("phase30 interface-changing report-target executable run should pass:\n" + interface_report_output);
    }
    ExpectIssueRollupRunOutput(interface_report_output,
                               "issue-rollup-attention\n",
                               "phase30 issue rollup interface-changing report-target executable run");
}

void TestIssueRollupImportedAggregateConstPressure(const std::filesystem::path& source_root,
                                                   const std::filesystem::path& binary_root,
                                                   const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = source_root / "examples/real/issue_rollup";
    const std::filesystem::path cloned_project_root = binary_root / "issue_rollup_aggregate_const_clone";
    CopyDirectoryTree(project_root, cloned_project_root);
    WriteFile(cloned_project_root / "build.toml",
              "schema = 1\n"
              "project = \"issue-rollup\"\n"
              "default = \"issue-rollup\"\n"
              "\n"
              "[targets.issue-rollup-core]\n"
              "kind = \"staticlib\"\n"
              "package = \"issue-rollup\"\n"
              "root = \"src/core/rollup_core.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"hosted\"\n"
              "\n"
              "[targets.issue-rollup-core.search_paths]\n"
              "modules = [\"src/core\", \"src/model\", \"src/parse\", \"src/render\", \"" +
                  (source_root / "stdlib").generic_string() + "\"]\n"
              "\n"
              "[targets.issue-rollup-core.runtime]\n"
              "startup = \"default\"\n"
              "\n"
              "[targets.issue-rollup]\n"
              "kind = \"exe\"\n"
              "package = \"issue-rollup\"\n"
              "root = \"src/app/main.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"hosted\"\n"
              "links = [\"issue-rollup-core\"]\n"
              "\n"
              "[targets.issue-rollup.search_paths]\n"
              "modules = [\"src/app\", \"src/core\", \"src/model\", \"src/parse\", \"src/render\", \"" +
                  (source_root / "stdlib").generic_string() + "\"]\n"
              "\n"
              "[targets.issue-rollup.runtime]\n"
              "startup = \"default\"\n"
              "\n"
              "[targets.issue-rollup.tests]\n"
              "enabled = true\n"
              "roots = [\"tests\"]\n"
              "mode = \"checked\"\n"
              "timeout_ms = 5000\n");
    WriteFile(cloned_project_root / "src/model/rollup_model.mc",
              "struct Summary {\n"
              "    open_items: usize\n"
              "    closed_items: usize\n"
              "    blocked_items: usize\n"
              "    priority_items: usize\n"
              "}\n"
              "\n"
              "const DEFAULT_SUMMARY: Summary = Summary{ open_items: 1, closed_items: 1, blocked_items: 0, priority_items: 0 }\n"
              "\n"
              "func total_items(summary: Summary) usize {\n"
              "    return summary.open_items + summary.closed_items + summary.blocked_items\n"
              "}\n"
              "\n"
              "func has_priority(summary: Summary) bool {\n"
              "    return summary.priority_items > 0\n"
              "}\n");
    WriteFile(cloned_project_root / "src/app/main.mc",
              "import fs\n"
              "import mem\n"
              "import rollup_core\n"
              "import rollup_model\n"
              "\n"
              "const DEFAULT_SUMMARY_COPY: rollup_model.Summary = rollup_model.DEFAULT_SUMMARY\n"
              "\n"
              "func main(args: Slice<cstr>) i32 {\n"
              "    if args.len != 2 {\n"
              "        return 64\n"
              "    }\n"
              "\n"
              "    buf: *Buffer<u8> = fs.read_all(args[1], mem.default_allocator())\n"
              "    if buf == nil {\n"
              "        return 92\n"
              "    }\n"
              "    defer mem.buffer_free<u8>(buf)\n"
              "\n"
              "    bytes: Slice<u8> = mem.slice_from_buffer<u8>(buf)\n"
              "    text: str = str{ ptr: bytes.ptr, len: bytes.len }\n"
              "    return rollup_core.write_text_rollup(text)\n"
              "}\n");

    const std::filesystem::path cloned_project_path = cloned_project_root / "build.toml";
    const std::filesystem::path check_build_dir = binary_root / "issue_rollup_aggregate_const_check_build";
    std::filesystem::remove_all(check_build_dir);

    const std::string output = CheckProjectTargetAndExpectSuccess(mc_path,
                                                                  cloned_project_path,
                                                                  check_build_dir,
                                                                  "",
                                                                  "issue_rollup_aggregate_const_check_output.txt",
                                                                  "issue rollup imported aggregate const pressure");
    ExpectOutputContains(output,
                         "checked target issue-rollup",
                         "phase42 issue rollup imported aggregate const pressure should succeed through project check");

    const std::filesystem::path build_dir = binary_root / "issue_rollup_aggregate_const_build";
    std::filesystem::remove_all(build_dir);

    BuildProjectTargetAndExpectSuccess(mc_path,
                                       cloned_project_path,
                                       build_dir,
                                       "",
                                       "issue_rollup_aggregate_const_build_output.txt",
                                       "issue rollup aggregate const executable build");

    const std::string run_output = RunProjectTargetAndExpectSuccess(mc_path,
                                                                    cloned_project_path,
                                                                    build_dir,
                                                                    "",
                                                                    cloned_project_root / "tests/sample.txt",
                                                                    "issue_rollup_aggregate_const_run_output.txt",
                                                                    "issue rollup aggregate const executable run");
    ExpectIssueRollupRunOutput(run_output,
                               "issue-rollup-steady\n",
                               "phase58 issue rollup aggregate const executable run");
}

}  // namespace

namespace mc::tool_tests {

namespace {

struct RealProjectTestCase {
    std::string_view name;
    void (*fn)(const std::filesystem::path&, const std::filesystem::path&, const std::filesystem::path&);
};

constexpr RealProjectTestCase kRealProjectTestCases[] = {
    {"grep-lite", &TestRealGrepLiteProject},
    {"file-walker", &TestRealFileWalkerProject},
    {"hash-tool", &TestRealHashToolProject},
    {"arena-expr", &TestRealArenaExprToolProject},
    {"worker-queue", &TestRealWorkerQueueProject},
    {"pipe-handoff", &TestRealPipeHandoffProject},
    {"pipe-ready", &TestRealPipeReadyProject},
    {"line-filter-relay", &TestRealLineFilterRelayProject},
    {"evented-echo", &TestRealEventedEchoProject},
    {"review-board", &TestRealReviewBoardProject},
    {"issue-rollup", &TestRealIssueRollupProject},
    {"issue-rollup-imported-aggregate-const", &TestIssueRollupImportedAggregateConstPressure},
};

const RealProjectTestCase* FindRealProjectTestCase(std::string_view case_name) {
    for (const auto& test_case : kRealProjectTestCases) {
        if (test_case.name == case_name) {
            return &test_case;
        }
    }
    return nullptr;
}

}  // namespace

void RunRealProjectToolSuite(const std::filesystem::path& source_root,
                             const std::filesystem::path& binary_root,
                             const std::filesystem::path& mc_path) {
    const std::filesystem::path suite_root = binary_root / "tool" / "real_projects";

    TestRealGrepLiteProject(source_root, suite_root, mc_path);
    TestRealFileWalkerProject(source_root, suite_root, mc_path);
    TestRealHashToolProject(source_root, suite_root, mc_path);
    TestRealArenaExprToolProject(source_root, suite_root, mc_path);
    TestRealWorkerQueueProject(source_root, suite_root, mc_path);
    TestRealPipeHandoffProject(source_root, suite_root, mc_path);
    TestRealPipeReadyProject(source_root, suite_root, mc_path);
    TestRealLineFilterRelayProject(source_root, suite_root, mc_path);
    TestRealEventedEchoProject(source_root, suite_root, mc_path);
    TestRealReviewBoardProject(source_root, suite_root, mc_path);
    TestRealIssueRollupProject(source_root, suite_root, mc_path);
    TestIssueRollupImportedAggregateConstPressure(source_root, suite_root, mc_path);
}

void RunRealProjectToolSuiteCase(const std::filesystem::path& source_root,
                                 const std::filesystem::path& binary_root,
                                 const std::filesystem::path& mc_path,
                                 std::string_view case_name) {
    const RealProjectTestCase* test_case = FindRealProjectTestCase(case_name);
    if (test_case == nullptr) {
        Fail("unknown real-project case selector: " + std::string(case_name));
    }
    test_case->fn(source_root, binary_root, mc_path);
}

}  // namespace mc::tool_tests
