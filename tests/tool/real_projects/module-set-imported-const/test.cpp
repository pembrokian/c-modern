#include <filesystem>
#include <string>

#include "tests/support/process_utils.h"
#include "tests/tool/tool_real_project_suite_internal.h"
#include "tests/tool/tool_suite_common.h"

namespace {

using mc::test_support::ExpectOutputContains;
using mc::test_support::WriteFile;
using mc::tool_tests::BuildProjectTargetAndExpectSuccess;
using mc::tool_tests::CheckProjectTargetAndExpectSuccess;
using mc::tool_tests::RunProjectTargetAndExpectSuccess;

}  // namespace

namespace mc::tool_tests {

void TestModuleSetImportedConstFollowThrough(const std::filesystem::path& source_root,
                                             const std::filesystem::path& binary_root,
                                             const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "module_set_imported_const_project";
    std::filesystem::remove_all(project_root);

    WriteFile(project_root / "build.toml",
              "schema = 1\n"
              "project = \"phase163-module-set-imported-const\"\n"
              "default = \"app\"\n"
              "\n"
              "[targets.app]\n"
              "kind = \"exe\"\n"
              "root = \"src/main.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"hosted\"\n"
              "\n"
              "[targets.app.search_paths]\n"
              "modules = [\"src\", \"" + (source_root / "stdlib").generic_string() + "\"]\n"
              "\n"
              "[targets.app.runtime]\n"
              "startup = \"default\"\n"
              "\n"
              "[targets.app.module_sets.helper]\n"
              "files = [\n"
              "  \"src/helper/00_types.mc\",\n"
              "  \"src/helper/01_consts.mc\",\n"
              "]\n");
    WriteFile(project_root / "src/helper/00_types.mc",
              "struct Pair {\n"
              "    left: i32\n"
              "    right: i32\n"
              "}\n");
    WriteFile(project_root / "src/helper/01_consts.mc",
              "const LIMIT: i32 = 9\n"
              "const DEFAULT_PAIR: Pair = Pair{ left: LIMIT, right: 5 }\n");
    WriteFile(project_root / "src/main.mc",
              "import helper\n"
              "\n"
              "const DOUBLE_LIMIT: i32 = helper.LIMIT + helper.LIMIT\n"
              "const LIMIT_PAIR: helper.Pair = helper.Pair{ left: helper.LIMIT, right: 5 }\n"
              "const DEFAULT_PAIR_COPY: helper.Pair = helper.DEFAULT_PAIR\n"
              "\n"
              "func main() i32 {\n"
              "    if helper.LIMIT != 9 {\n"
              "        return 81\n"
              "    }\n"
              "    if DOUBLE_LIMIT != 18 {\n"
              "        return 82\n"
              "    }\n"
              "    if LIMIT_PAIR.left != 9 || LIMIT_PAIR.right != 5 {\n"
              "        return 83\n"
              "    }\n"
              "    if DEFAULT_PAIR_COPY.left != 9 || DEFAULT_PAIR_COPY.right != 5 {\n"
              "        return 84\n"
              "    }\n"
              "    return 0\n"
              "}\n");

    const std::filesystem::path project_path = project_root / "build.toml";
    const std::filesystem::path check_build_dir = binary_root / "module_set_imported_const_check_build";
    std::filesystem::remove_all(check_build_dir);

    const std::string check_output = CheckProjectTargetAndExpectSuccess(mc_path,
                                                                        project_path,
                                                                        check_build_dir,
                                                                        "",
                                                                        "module_set_imported_const_check_output.txt",
                                                                        "module-set imported const follow-through check");
    ExpectOutputContains(check_output,
                         "checked target app",
                         "phase163 module-set imported const follow-through should succeed through project check");

    const std::filesystem::path build_dir = binary_root / "module_set_imported_const_build";
    std::filesystem::remove_all(build_dir);

    BuildProjectTargetAndExpectSuccess(mc_path,
                                       project_path,
                                       build_dir,
                                       "",
                                       "module_set_imported_const_build_output.txt",
                                       "module-set imported const follow-through build");

    RunProjectTargetAndExpectSuccess(mc_path,
                                     project_path,
                                     build_dir,
                                     "",
                                     project_path,
                                     "module_set_imported_const_run_output.txt",
                                     "module-set imported const follow-through run");
}

}  // namespace mc::tool_tests