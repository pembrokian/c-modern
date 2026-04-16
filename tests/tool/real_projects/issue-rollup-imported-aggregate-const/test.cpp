#include <filesystem>
#include <string>
#include <string_view>

#include "tests/support/process_utils.h"
#include "tests/tool/tool_real_project_suite_internal.h"
#include "tests/tool/tool_suite_common.h"

namespace {

using mc::test_support::CopyDirectoryTree;
using mc::test_support::ExpectOutputContains;
using mc::test_support::WriteFile;
using mc::tool_tests::BuildProjectTargetAndExpectSuccess;
using mc::tool_tests::CheckProjectTargetAndExpectSuccess;
using mc::tool_tests::RunProjectTargetAndExpectSuccess;

void ExpectIssueRollupRunOutput(std::string_view output,
                                std::string_view expected_line,
                                const std::string& context_prefix) {
    ExpectOutputContains(output,
                         expected_line,
                         context_prefix + ": should print the deterministic grouped-package result");
}

}  // namespace

namespace mc::tool_tests {

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

}  // namespace mc::tool_tests