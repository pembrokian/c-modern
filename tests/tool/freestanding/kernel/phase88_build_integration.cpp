#include <filesystem>
#include <string>

#include "compiler/support/dump_paths.h"
#include "compiler/support/target.h"
#include "tests/support/process_utils.h"
#include "tests/tool/tool_suite_common.h"

namespace mc::tool_tests {

using mc::test_support::ExpectOutputContains;
using mc::test_support::Fail;
using mc::test_support::ReadFile;
using mc::test_support::RunCommandCapture;
using mc::test_support::WriteFile;
void RunFreestandingKernelPhase88BuildIntegrationAudit(const std::filesystem::path& source_root,
                                            const std::filesystem::path& binary_root,
                                            const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / "kernel_build_integration_project";
    std::filesystem::remove_all(project_root);

    WriteFile(project_root / "src/main.mc",
              "extern(c) func phase88_kernel_delta() i32\n"
              "\n"
              "func bootstrap_main() i32 {\n"
              "    return 80 + phase88_kernel_delta()\n"
              "}\n");
    WriteFile(project_root / "target/phase88_driver_support.c",
              "int phase88_kernel_delta(void) {\n"
              "    return 8;\n"
              "}\n");

    const std::filesystem::path driver_support_object = project_root / "target/phase88_driver_support.o";
    const auto [driver_support_compile_outcome, driver_support_compile_output] = RunCommandCapture({"xcrun",
                                                                                                    "clang",
                                                                                                    "-target",
                                                                                                    std::string(mc::kBootstrapTriple),
                                                                                                    "-c",
                                                                                                    (project_root / "target/phase88_driver_support.c").generic_string(),
                                                                                                    "-o",
                                                                                                    driver_support_object.generic_string()},
                                                                                                   project_root / "target/phase88_driver_support_compile_output.txt",
                                                                                                   "phase88 driver support object compile");
    if (!driver_support_compile_outcome.exited || driver_support_compile_outcome.exit_code != 0) {
        Fail("phase88 driver support object compile should pass:\n" + driver_support_compile_output);
    }

    WriteFile(project_root / "build.toml",
              "schema = 1\n"
              "project = \"phase88-kernel-build-integration\"\n"
              "default = \"kernel\"\n"
              "\n"
              "[targets.kernel]\n"
              "kind = \"exe\"\n"
              "package = \"phase88\"\n"
              "root = \"src/main.mc\"\n"
              "mode = \"debug\"\n"
              "env = \"freestanding\"\n"
              "target = \"" + std::string(mc::kBootstrapTargetFamily) + "\"\n"
              "\n"
              "[targets.kernel.search_paths]\n"
              "modules = [\"src\", \"" + (source_root / "stdlib").generic_string() + "\"]\n"
              "\n"
              "[targets.kernel.runtime]\n"
              "startup = \"bootstrap_main\"\n"
              "\n"
              "[targets.kernel.link]\n"
              "inputs = [\"target/phase88_driver_support.o\"]\n");

    const std::filesystem::path project_path = project_root / "build.toml";
    const std::filesystem::path build_dir = binary_root / "kernel_build_integration_build";
    MaybeCleanBuildDir(build_dir);

    BuildProjectTargetAndExpectSuccess(mc_path,
                                       project_path,
                                       build_dir,
                                       "kernel",
                                       "phase88_kernel_build_output.txt",
                                       "phase88 freestanding kernel build integration build");

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(project_root / "src/main.mc", build_dir);
    const std::filesystem::path runtime_object =
        build_targets.object.parent_path() /
        (build_targets.object.stem().generic_string() + ".freestanding.bootstrap_main.runtime.o");
    if (!std::filesystem::exists(build_targets.object)) {
        Fail("phase88 freestanding kernel build integration build should emit an entry object artifact");
    }
    if (!std::filesystem::exists(runtime_object)) {
        Fail("phase88 freestanding kernel build integration build should emit a freestanding runtime object artifact");
    }
    if (!std::filesystem::exists(build_targets.executable)) {
        Fail("phase88 freestanding kernel build integration build should emit an executable artifact");
    }

    const auto [driver_run_outcome, driver_run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                                           build_dir / "phase88_driver_link_run_output.txt",
                                                                           "phase88 freestanding driver-linked kernel run");
    if (!driver_run_outcome.exited || driver_run_outcome.exit_code != 88) {
        Fail("phase88 freestanding driver-linked kernel run should exit with the build-integration proof marker:\n" +
             driver_run_output);
    }

    WriteFile(project_root / "target/phase88_manual_support.c",
              "int phase88_kernel_delta(void) {\n"
              "    return 9;\n"
              "}\n");
    const std::filesystem::path manual_support_object = project_root / "target/phase88_manual_support.o";
    const auto [manual_support_compile_outcome, manual_support_compile_output] = RunCommandCapture({"xcrun",
                                                                                                    "clang",
                                                                                                    "-target",
                                                                                                    std::string(mc::kBootstrapTriple),
                                                                                                    "-c",
                                                                                                    (project_root / "target/phase88_manual_support.c").generic_string(),
                                                                                                    "-o",
                                                                                                    manual_support_object.generic_string()},
                                                                                                   project_root / "target/phase88_manual_support_compile_output.txt",
                                                                                                   "phase88 manual support object compile");
    if (!manual_support_compile_outcome.exited || manual_support_compile_outcome.exit_code != 0) {
        Fail("phase88 manual support object compile should pass:\n" + manual_support_compile_output);
    }

    const std::filesystem::path manual_executable = build_dir / "bin" / "phase88_manual_kernel";
    const auto [manual_link_outcome, manual_link_output] = RunCommandCapture({"xcrun",
                                                                              "clang",
                                                                              "-target",
                                                                              std::string(mc::kBootstrapTriple),
                                                                              build_targets.object.generic_string(),
                                                                              manual_support_object.generic_string(),
                                                                              runtime_object.generic_string(),
                                                                              "-o",
                                                                              manual_executable.generic_string()},
                                                                             build_dir / "phase88_manual_link_output.txt",
                                                                             "phase88 manual freestanding kernel link");
    if (!manual_link_outcome.exited || manual_link_outcome.exit_code != 0) {
        Fail("phase88 manual freestanding kernel link should succeed:\n" + manual_link_output);
    }

    const auto [manual_run_outcome, manual_run_output] = RunCommandCapture({manual_executable.generic_string()},
                                                                           build_dir / "phase88_manual_link_run_output.txt",
                                                                           "phase88 manual freestanding kernel run");
    if (!manual_run_outcome.exited || manual_run_outcome.exit_code != 89) {
        Fail("phase88 manual freestanding kernel run should exit with the relinked proof marker:\n" +
             manual_run_output);
    }

}

}  // namespace mc::tool_tests
