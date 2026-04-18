#include <cstdint>
#include <filesystem>
#include <functional>
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
using mc::test_support::ReadExactFromSocket;
using mc::test_support::SpawnBackgroundCommandWithInheritedLoopbackListener;
using mc::test_support::WriteAllToSocket;
using mc::tool_tests::RunProjectTestAndExpectSuccess;

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
                                  std::string_view output_name,
                                  const std::string& context) {
    uint16_t port = 0;
    const BackgroundProcess run_process = SpawnBackgroundCommandWithInheritedLoopbackListener(
        {mc_path.generic_string(),
         "run",
         "--project",
         project_path.generic_string(),
         "--build-dir",
         build_dir.generic_string(),
         "--"},
        build_dir / std::string(output_name),
        context,
        &port);

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
                                   std::string_view output_name,
                                   const std::string& context) {
    uint16_t port = 0;
    const BackgroundProcess run_process = SpawnBackgroundCommandWithInheritedLoopbackListener(
        {mc_path.generic_string(),
         "run",
         "--project",
         project_path.generic_string(),
         "--build-dir",
         build_dir.generic_string(),
         "--"},
        build_dir / std::string(output_name),
        context,
        &port);

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
                         context_prefix + ": should print the ordinary-test verdict");
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
                         context_prefix + ": should print the ordinary-test verdict");
    ExpectOutputContains(output,
                         "PASS target partial-write",
                         context_prefix + ": should print the target verdict");
}

void ExerciseProjectRunTestRerunSequence(const std::filesystem::path& mc_path,
                                         const std::filesystem::path& project_path,
                                         const std::filesystem::path& build_dir,
                                         const std::function<void(std::string_view, const std::string&)>& run_round_trip,
                                         const std::function<void(std::string_view, const std::string&)>& verify_test_output,
                                         std::string_view first_run_output_name,
                                         std::string_view test_output_name,
                                         std::string_view second_run_output_name,
                                         const std::string& context_prefix) {
    run_round_trip(first_run_output_name, context_prefix + " run before tests");
    const std::string test_output = RunProjectTestAndExpectSuccess(mc_path,
                                                                   project_path,
                                                                   build_dir,
                                                                   test_output_name,
                                                                   context_prefix + " test after run");
    verify_test_output(test_output, context_prefix + " test after run");
    run_round_trip(second_run_output_name, context_prefix + " rerun after tests");
}

void ExerciseProjectTestRunRerunSequence(const std::filesystem::path& mc_path,
                                         const std::filesystem::path& project_path,
                                         const std::filesystem::path& build_dir,
                                         const std::function<void(std::string_view, const std::string&)>& run_round_trip,
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
    run_round_trip(run_output_name, context_prefix + " run after tests");
    const std::string second_test_output = RunProjectTestAndExpectSuccess(mc_path,
                                                                          project_path,
                                                                          build_dir,
                                                                          second_test_output_name,
                                                                          context_prefix + " retest after run");
    verify_test_output(second_test_output, context_prefix + " retest after run");
}

}  // namespace

namespace mc::tool_tests {

void TestRealEventedEchoProject(const std::filesystem::path& source_root,
                                const std::filesystem::path& binary_root,
                                const std::filesystem::path& mc_path) {
    const std::filesystem::path project_path = source_root / "examples/real/evented_echo/build.toml";
    const std::filesystem::path run_test_rerun_build_dir = binary_root / "evented_echo_run_test_rerun_build";
    std::filesystem::remove_all(run_test_rerun_build_dir);
    ExerciseProjectRunTestRerunSequence(mc_path,
                                        project_path,
                                        run_test_rerun_build_dir,
                                        [&](std::string_view output_name, const std::string& context) {
                                            ExerciseEventedEchoRoundTrip(mc_path,
                                                                         project_path,
                                                                         run_test_rerun_build_dir,
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
                                        [&](std::string_view output_name, const std::string& context) {
                                            ExerciseEventedEchoRoundTrip(mc_path,
                                                                         project_path,
                                                                         test_run_rerun_build_dir,
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
                                        [&](std::string_view output_name, const std::string& context) {
                                            ExercisePartialWriteRoundTrip(mc_path,
                                                                          partial_write_project_path,
                                                                          partial_write_run_test_rerun_build_dir,
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
                                        [&](std::string_view output_name, const std::string& context) {
                                            ExercisePartialWriteRoundTrip(mc_path,
                                                                          partial_write_project_path,
                                                                          partial_write_test_run_rerun_build_dir,
                                                                          output_name,
                                                                          context);
                                        },
                                        ExpectPartialWriteTestOutput,
                                        "evented_partial_write_initial_test_output.txt",
                                        "evented_partial_write_run_after_test_output.txt",
                                        "evented_partial_write_retest_output.txt",
                                        "evented partial-write test-run-rerun");
}

}  // namespace mc::tool_tests