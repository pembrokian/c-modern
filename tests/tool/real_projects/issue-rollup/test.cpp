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
using mc::tool_tests::BuildProjectTargetAndCapture;
using mc::tool_tests::BuildProjectTargetAndExpectSuccess;
using mc::tool_tests::RunProjectTargetAndExpectFailure;
using mc::tool_tests::RunProjectTargetAndExpectSuccess;
using mc::tool_tests::RunProjectTestAndExpectSuccess;

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

}  // namespace

namespace mc::tool_tests {

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
    const auto rollup_core_object_time_2 = RequireWriteTime(rollup_core_object);
    const auto app_main_object_time_2 = RequireWriteTime(app_main_object);
    const auto report_main_object_time_2 = RequireWriteTime(report_main_object);
    const std::string rollup_model_mci_text_2 = ReadFile(rollup_model_mci);
    const std::string rollup_parse_mci_text_2 = ReadFile(rollup_parse_mci);
    const std::string rollup_render_mci_text_2 = ReadFile(rollup_render_mci);
    const std::string rollup_core_mci_text_2 = ReadFile(rollup_core_mci);
    const auto rollup_core_mci_time_2 = RequireWriteTime(rollup_core_mci);
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
    const auto rollup_core_object_time_3 = RequireWriteTime(rollup_core_object);
    const auto app_main_object_time_3 = RequireWriteTime(app_main_object);
    const auto report_main_object_time_3 = RequireWriteTime(report_main_object);
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

}  // namespace mc::tool_tests