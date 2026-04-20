#include <filesystem>
#include <string>
#include <string_view>

#include "tests/support/process_utils.h"
#include "tests/tool/tool_real_project_suite_internal.h"
#include "tests/tool/tool_suite_common.h"

namespace {

using mc::test_support::ExpectOutputContains;
using mc::tool_tests::RunProjectAndExpectSuccess;
using mc::tool_tests::RunProjectTestAndExpectSuccess;

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

}  // namespace

namespace mc::tool_tests {

void TestRealPipeHandoffProject(const std::filesystem::path& source_root,
                                const std::filesystem::path& binary_root,
                                const std::filesystem::path& mc_path) {
    const std::filesystem::path project_path = source_root / "examples/real/pipe_handoff/build.toml";
    const std::filesystem::path run_test_rerun_build_dir = binary_root / "pipe_handoff_run_test_rerun_build";
    std::filesystem::remove_all(run_test_rerun_build_dir);

    std::string run_output = RunProjectAndExpectSuccess(mc_path,
                                                        project_path,
                                                        run_test_rerun_build_dir,
                                                        {},
                                                        "pipe_handoff_run_output.txt",
                                                        "pipe handoff run before tests");
    ExpectPipeHandoffRunOutput(run_output,
                               "phase43 pipe handoff run before tests");

    std::string test_output = RunProjectTestAndExpectSuccess(mc_path,
                                                             project_path,
                                                             run_test_rerun_build_dir,
                                                             "pipe_handoff_test_output.txt",
                                                             "pipe handoff test after run");
    ExpectPipeHandoffTestOutput(test_output,
                                "phase43 pipe handoff test after run");

    run_output = RunProjectAndExpectSuccess(mc_path,
                                            project_path,
                                            run_test_rerun_build_dir,
                                            {},
                                            "pipe_handoff_rerun_output.txt",
                                            "pipe handoff rerun after tests");
    ExpectPipeHandoffRunOutput(run_output,
                               "phase43 pipe handoff rerun after tests");

    const std::filesystem::path test_run_rerun_build_dir = binary_root / "pipe_handoff_test_run_rerun_build";
    std::filesystem::remove_all(test_run_rerun_build_dir);

    test_output = RunProjectTestAndExpectSuccess(mc_path,
                                                 project_path,
                                                 test_run_rerun_build_dir,
                                                 "pipe_handoff_initial_test_output.txt",
                                                 "pipe handoff initial test");
    ExpectPipeHandoffTestOutput(test_output,
                                "phase43 pipe handoff initial test");

    run_output = RunProjectAndExpectSuccess(mc_path,
                                            project_path,
                                            test_run_rerun_build_dir,
                                            {},
                                            "pipe_handoff_run_after_test_output.txt",
                                            "pipe handoff run after tests");
    ExpectPipeHandoffRunOutput(run_output,
                               "phase43 pipe handoff run after tests");

    test_output = RunProjectTestAndExpectSuccess(mc_path,
                                                 project_path,
                                                 test_run_rerun_build_dir,
                                                 "pipe_handoff_retest_output.txt",
                                                 "pipe handoff retest after run");
    ExpectPipeHandoffTestOutput(test_output,
                                "phase43 pipe handoff retest after run");
}

}  // namespace mc::tool_tests