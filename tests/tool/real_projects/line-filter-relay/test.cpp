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