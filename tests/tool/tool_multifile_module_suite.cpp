#include <filesystem>
#include <string>

#include "tests/support/process_utils.h"
#include "tests/tool/tool_suite_common.h"
#include "tests/tool/tool_workflow_suite_internal.h"

namespace {

using mc::test_support::ExpectOutputContains;
using mc::test_support::Fail;
using mc::test_support::RunCommandCapture;
using mc::test_support::WriteFile;
using namespace mc::tool_tests;
using mc::tool_tests::BuildProjectTargetAndExpectFailure;

void TestProjectModeMultiFileModulesBuildAndRun(const std::filesystem::path& binary_root,
                                                const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "multifile_project";
    std::filesystem::remove_all(project_root);
    WriteFile(project_root / "build.toml",
              "schema = 1\n"
              "project = \"phase127-multifile-success\"\n"
              "default = \"app\"\n"
              "\n"
              "[targets.app]\n"
              "kind = \"exe\"\n"
              "package = \"app\"\n"
              "root = \"src/main.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"hosted\"\n"
              "\n"
              "[targets.app.search_paths]\n"
              "modules = [\"src\"]\n"
              "\n"
              "[targets.app.module_sets.main]\n"
              "files = [\n"
              "  \"src/main.mc\",\n"
              "  \"src/main_logic.mc\"\n"
              "]\n"
              "\n"
              "[targets.app.module_sets.helper]\n"
              "files = [\n"
              "  \"src/helper_public.mc\",\n"
              "  \"src/helper_private.mc\",\n"
              "  \"src/helper_extra.mc\"\n"
              "]\n"
              "\n"
              "[targets.app.runtime]\n"
              "startup = \"default\"\n");
    WriteFile(project_root / "src/main.mc",
              "import helper\n"
              "\n"
              "func main() i32 {\n"
              "    return helper.answer() + module_bonus()\n"
              "}\n");
    WriteFile(project_root / "src/main_logic.mc",
              "@private\n"
              "func hidden_bonus() i32 {\n"
              "    return 1\n"
              "}\n"
              "\n"
              "func module_bonus() i32 {\n"
              "    return hidden_bonus() + 1\n"
              "}\n");
    WriteFile(project_root / "src/helper_private.mc",
              "@private\n"
              "func hidden_value() i32 {\n"
              "    return 3\n"
              "}\n");
    WriteFile(project_root / "src/helper_extra.mc",
              "func extra_value() i32 {\n"
              "    return 5\n"
              "}\n");
    WriteFile(project_root / "src/helper_public.mc",
              "func answer() i32 {\n"
              "    return hidden_value() + extra_value()\n"
              "}\n");

    const std::filesystem::path build_dir = binary_root / "multifile_build";
    std::filesystem::remove_all(build_dir);
    BuildProjectTargetAndExpectSuccess(mc_path,
                                       project_root / "build.toml",
                                       build_dir,
                                       "app",
                                       "multifile_build_output.txt",
                                       "multi-file module build");

    const auto [run_outcome, run_output] = RunCommandCapture({mc_path.generic_string(),
                                                              "run",
                                                              "--project",
                                                              (project_root / "build.toml").generic_string(),
                                                              "--build-dir",
                                                              build_dir.generic_string()},
                                                             build_dir / "multifile_run_output.txt",
                                                             "multi-file module run");
    if (!run_outcome.exited || run_outcome.exit_code != 10) {
        Fail("multi-file module project should return 10, got:\n" + run_output);
    }
}

void TestDuplicateModuleSetFileOwnershipFails(const std::filesystem::path& binary_root,
                                              const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "multifile_duplicate_ownership_project";
    std::filesystem::remove_all(project_root);
    WriteFile(project_root / "build.toml",
              "schema = 1\n"
              "project = \"phase127-duplicate-ownership\"\n"
              "default = \"app\"\n"
              "\n"
              "[targets.app]\n"
              "kind = \"exe\"\n"
              "package = \"app\"\n"
              "root = \"src/main.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"hosted\"\n"
              "\n"
              "[targets.app.search_paths]\n"
              "modules = [\"src\"]\n"
              "\n"
              "[targets.app.module_sets.main]\n"
              "files = [\"src/main.mc\", \"src/shared.mc\"]\n"
              "\n"
              "[targets.app.module_sets.helper]\n"
              "files = [\"src/helper.mc\", \"src/shared.mc\"]\n"
              "\n"
              "[targets.app.runtime]\n"
              "startup = \"default\"\n");
    WriteFile(project_root / "src/main.mc", "func main() i32 { return 0 }\n");
    WriteFile(project_root / "src/shared.mc", "func shared() i32 { return 1 }\n");
    WriteFile(project_root / "src/helper.mc", "func answer() i32 { return 2 }\n");

    const std::filesystem::path build_dir = binary_root / "multifile_duplicate_ownership_build";
    std::filesystem::remove_all(build_dir);
    const std::string output = BuildProjectTargetAndExpectFailure(mc_path,
                                                                  project_root / "build.toml",
                                                                  build_dir,
                                                                  "app",
                                                                  "multifile_duplicate_ownership_output.txt",
                                                                  "duplicate module-set ownership build");
    ExpectOutputContains(output,
                         "assigns source file",
                         "duplicate module-set file ownership should be rejected");
    ExpectOutputContains(output,
                         "multiple module sets",
                         "duplicate module-set file ownership diagnostic should explain the conflict");
}

void TestDuplicateTopLevelAcrossModulePartsFails(const std::filesystem::path& binary_root,
                                                 const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "multifile_duplicate_symbol_project";
    std::filesystem::remove_all(project_root);
    WriteFile(project_root / "build.toml",
              "schema = 1\n"
              "project = \"phase127-duplicate-symbol\"\n"
              "default = \"app\"\n"
              "\n"
              "[targets.app]\n"
              "kind = \"exe\"\n"
              "package = \"app\"\n"
              "root = \"src/main.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"hosted\"\n"
              "\n"
              "[targets.app.search_paths]\n"
              "modules = [\"src\"]\n"
              "\n"
              "[targets.app.module_sets.helper]\n"
              "files = [\"src/helper_a.mc\", \"src/helper_b.mc\"]\n"
              "\n"
              "[targets.app.runtime]\n"
              "startup = \"default\"\n");
    WriteFile(project_root / "src/main.mc",
              "import helper\n"
              "\n"
              "func main() i32 {\n"
              "    return helper.answer()\n"
              "}\n");
    WriteFile(project_root / "src/helper_a.mc",
              "func answer() i32 {\n"
              "    return 1\n"
              "}\n");
    WriteFile(project_root / "src/helper_b.mc",
              "func answer() i32 {\n"
              "    return 2\n"
              "}\n");

    const std::filesystem::path build_dir = binary_root / "multifile_duplicate_symbol_build";
    std::filesystem::remove_all(build_dir);
    const std::string output = BuildProjectTargetAndExpectFailure(mc_path,
                                                                  project_root / "build.toml",
                                                                  build_dir,
                                                                  "app",
                                                                  "multifile_duplicate_symbol_output.txt",
                                                                  "duplicate symbol across module parts build");
    ExpectOutputContains(output,
                         "duplicate top-level value symbol: answer",
                         "duplicate top-level declarations across module parts should fail deterministically");
    ExpectOutputContains(output,
                         "helper_b.mc",
                         "duplicate-symbol diagnostic should name the contributing part file, not the primary part file");
}

}  // namespace

namespace mc::tool_tests {

void RunWorkflowMultifileModuleSuite(const std::filesystem::path& binary_root,
                                     const std::filesystem::path& mc_path) {
    TestProjectModeMultiFileModulesBuildAndRun(binary_root, mc_path);
    TestDuplicateModuleSetFileOwnershipFails(binary_root, mc_path);
    TestDuplicateTopLevelAcrossModulePartsFails(binary_root, mc_path);
}

}  // namespace mc::tool_tests
