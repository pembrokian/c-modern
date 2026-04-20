#include <filesystem>
#include <string>
#include <string_view>

#include "compiler/support/dump_paths.h"
#include "tests/support/process_utils.h"
#include "tests/tool/tool_real_project_suite_internal.h"
#include "tests/tool/tool_suite_common.h"

namespace {

using mc::test_support::CopyDirectoryTree;
using mc::test_support::ExpectOutputContains;
using mc::test_support::Fail;
using mc::test_support::ReadFile;
using mc::test_support::RequireWriteTime;
using mc::test_support::RunCommandCapture;
using mc::test_support::SleepForTimestampTick;
using mc::test_support::WriteFile;
using mc::tool_tests::BuildProjectTargetAndExpectSuccess;
using mc::tool_tests::RunProjectTargetAndExpectSuccess;
using mc::tool_tests::RunProjectTestTargetAndExpectSuccess;

void ExpectReviewBoardRunOutput(std::string_view output,
                                std::string_view expected_line,
                                const std::string& context_prefix) {
    ExpectOutputContains(output,
                         expected_line,
                         context_prefix + ": should print the deterministic target result");
}

void ExpectReviewBoardUiOutput(std::string_view output,
                               std::string_view expected_line,
                               const std::string& context_prefix) {
    ExpectOutputContains(output,
                         expected_line,
                         context_prefix + ": should print the deterministic ui surface");
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

}  // namespace

namespace mc::tool_tests {

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

    const auto [ui_run_outcome, ui_run_output] = RunCommandCapture({mc_path.generic_string(),
                                                                    "run",
                                                                    "--project",
                                                                    project_path.generic_string(),
                                                                    "--target",
                                                                    "ui",
                                                                    "--build-dir",
                                                                    graph_build_dir.generic_string(),
                                                                    "--",
                                                                    "OTFTUUU"},
                                                                   graph_build_dir / "review_board_ui_run_output.txt",
                                                                   "review board explicit ui run");
    if (!ui_run_outcome.exited || ui_run_outcome.exit_code != 0) {
        Fail("review board explicit ui run should pass:\n" + ui_run_output);
    }
    ExpectReviewBoardUiOutput(ui_run_output,
                              "FEFB\n",
                              "phase286 review board explicit ui run");

    const auto [ui_detail_run_outcome, ui_detail_run_output] = RunCommandCapture({mc_path.generic_string(),
                                                                                  "run",
                                                                                  "--project",
                                                                                  project_path.generic_string(),
                                                                                  "--target",
                                                                                  "ui",
                                                                                  "--build-dir",
                                                                                  graph_build_dir.generic_string(),
                                                                                  "--",
                                                                                  "DDDOTFTUUU"},
                                                                                 graph_build_dir / "review_board_ui_detail_run_output.txt",
                                                                                 "review board explicit ui detail run");
    if (!ui_detail_run_outcome.exited || ui_detail_run_outcome.exit_code != 0) {
        Fail("review board explicit ui detail run should pass:\n" + ui_detail_run_output);
    }
    ExpectReviewBoardUiOutput(ui_detail_run_output,
                              "UEFB\n",
                              "phase296 review board explicit ui detail run");

    const auto [ui_filter_prompt_outcome, ui_filter_prompt_output] = RunCommandCapture({mc_path.generic_string(),
                                                                                       "run",
                                                                                       "--project",
                                                                                       project_path.generic_string(),
                                                                                       "--target",
                                                                                       "ui",
                                                                                       "--build-dir",
                                                                                       graph_build_dir.generic_string(),
                                                                                       "--",
                                                                                       "TQU"},
                                                                                      graph_build_dir / "review_board_ui_filter_prompt_run_output.txt",
                                                                                      "review board explicit ui filter prompt run");
    if (!ui_filter_prompt_outcome.exited || ui_filter_prompt_outcome.exit_code != 0) {
        Fail("review board explicit ui filter prompt run should pass:\n" + ui_filter_prompt_output);
    }
    ExpectReviewBoardUiOutput(ui_filter_prompt_output,
                              "U-AS\n",
                              "phase298 review board explicit ui filter prompt run");

    const auto [ui_filter_run_outcome, ui_filter_run_output] = RunCommandCapture({mc_path.generic_string(),
                                                                                  "run",
                                                                                  "--project",
                                                                                  project_path.generic_string(),
                                                                                  "--target",
                                                                                  "ui",
                                                                                  "--build-dir",
                                                                                  graph_build_dir.generic_string(),
                                                                                  "--",
                                                                                  "TQUR"},
                                                                                 graph_build_dir / "review_board_ui_filter_run_output.txt",
                                                                                 "review board explicit ui filter run");
    if (!ui_filter_run_outcome.exited || ui_filter_run_outcome.exit_code != 0) {
        Fail("review board explicit ui filter run should pass:\n" + ui_filter_run_output);
    }
    ExpectReviewBoardUiOutput(ui_filter_run_output,
                              "UQAS\n",
                              "phase298 review board explicit ui filter run");

    const auto [ui_parity_run_outcome, ui_parity_run_output] = RunCommandCapture({mc_path.generic_string(),
                                                                                  "run",
                                                                                  "--project",
                                                                                  project_path.generic_string(),
                                                                                  "--target",
                                                                                  "ui",
                                                                                  "--build-dir",
                                                                                  graph_build_dir.generic_string(),
                                                                                  "--",
                                                                                  "OTFTUUUDDDTQAQQ"},
                                                                                 graph_build_dir / "review_board_ui_parity_run_output.txt",
                                                                                 "review board richer ui parity run");
    if (!ui_parity_run_outcome.exited || ui_parity_run_outcome.exit_code != 0) {
        Fail("review board richer ui parity run should pass:\n" + ui_parity_run_output);
    }
    ExpectReviewBoardUiOutput(ui_parity_run_output,
                              "UEFS\n",
                              "phase299 review board richer ui parity run");

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
              "timeout_ms = 5000\n"
              "\n"
              "[targets.ui]\n"
              "kind = \"exe\"\n"
              "package = \"review-board\"\n"
              "root = \"src/ui_main.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"hosted\"\n"
              "\n"
              "[targets.ui.search_paths]\n"
              "modules = [\"src\", \"" + (source_root / "stdlib").generic_string() + "\"]\n"
              "\n"
              "[targets.ui.runtime]\n"
              "startup = \"default\"\n");
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
    if (!(audit_main_object_time_2 > audit_main_object_time_1)) {
        Fail("phase22 implementation-only internal scan edit should rebuild the default-target main object");
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
    if (!(RequireWriteTime(focus_main_object) > focus_main_object_time_1)) {
        Fail("phase22 implementation-only focus rebuild should rebuild the explicit-target main object");
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
    if (!(focus_main_object_time_3 > focus_main_object_time_1)) {
        Fail("phase22 interface-changing audit rebuild should rebuild the explicit-target main object before that target is selected");
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

}  // namespace mc::tool_tests