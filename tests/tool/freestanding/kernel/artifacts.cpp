#include <filesystem>
#include <string>
#include <vector>

#include "compiler/support/dump_paths.h"
#include "compiler/support/target.h"
#include "tests/support/process_utils.h"
#include "tests/tool/tool_suite_common.h"

namespace mc::tool_tests {

using mc::test_support::Fail;
using mc::test_support::WriteFile;

namespace {

void RunFreestandingKernelPhase88ManualRelinkAudit(const std::filesystem::path& source_root,
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
    CompileBootstrapCObjectAndExpectSuccess(project_root / "target/phase88_driver_support.c",
                                            driver_support_object,
                                            project_root / "target/phase88_driver_support_compile_output.txt",
                                            "phase88 artifact-suite driver support object compile");

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
                                       "phase88 artifact-suite freestanding kernel build integration build");

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(project_root / "src/main.mc", build_dir);
    const std::filesystem::path runtime_object =
        build_targets.object.parent_path() /
        (build_targets.object.stem().generic_string() + ".freestanding.bootstrap_main.runtime.o");
    if (!std::filesystem::exists(build_targets.object)) {
        Fail("phase88 artifact suite should emit an entry object artifact");
    }
    if (!std::filesystem::exists(runtime_object)) {
        Fail("phase88 artifact suite should emit a freestanding runtime object artifact");
    }

    WriteFile(project_root / "target/phase88_manual_support.c",
              "int phase88_kernel_delta(void) {\n"
              "    return 9;\n"
              "}\n");
    const std::filesystem::path manual_support_object = project_root / "target/phase88_manual_support.o";
    CompileBootstrapCObjectAndExpectSuccess(project_root / "target/phase88_manual_support.c",
                                            manual_support_object,
                                            project_root / "target/phase88_manual_support_compile_output.txt",
                                            "phase88 artifact-suite manual support object compile");

    const std::filesystem::path manual_executable = build_dir / "bin" / "phase88_manual_kernel";
    LinkBootstrapObjectsAndRunExpectExitCode({build_targets.object, manual_support_object, runtime_object},
                                             manual_executable,
                                             build_dir / "phase88_manual_link_output.txt",
                                             build_dir / "phase88_manual_link_run_output.txt",
                                             89,
                                             "phase88 manual freestanding kernel link",
                                             "phase88 manual freestanding kernel run");
}

void RunFreestandingKernelPhase108ManualRelinkAudit(const std::filesystem::path& source_root,
                                                    const std::filesystem::path& binary_root,
                                                    const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const auto artifacts = BuildAndRunFreestandingKernelTarget(mc_path,
                                                               common_paths.project_path,
                                                               common_paths.main_source_path,
                                                               binary_root / "kernel_build",
                                                               "kernel_phase108_artifacts",
                                                               "freestanding kernel phase108 artifact-suite build",
                                                               "freestanding kernel phase108 artifact-suite run");

    const std::filesystem::path runtime_object =
        artifacts.build_targets.object.parent_path() /
        (artifacts.build_targets.object.stem().generic_string() + ".freestanding.bootstrap_main.runtime.o");
    if (!std::filesystem::exists(artifacts.build_targets.object)) {
        Fail("phase108 kernel image audit should emit an entry object artifact for target-owned relink");
    }
    if (!std::filesystem::exists(runtime_object)) {
        Fail("phase108 kernel image audit should emit a freestanding runtime object artifact for target-owned relink");
    }
    if (!std::filesystem::exists(artifacts.build_targets.executable)) {
        Fail("phase108 kernel image audit should emit the linked kernel executable artifact");
    }

    const std::filesystem::path manual_executable = artifacts.build_dir / "bin" / "phase108_manual_kernel_image";
    const std::string manual_run_output = LinkBootstrapObjectsAndRunExpectExitCode(
        CollectBootstrapObjectFiles(artifacts.build_targets.object.parent_path()),
        manual_executable,
        artifacts.build_dir / "phase108_manual_link_output.txt",
        artifacts.build_dir / "phase108_manual_link_run_output.txt",
        0,
        "phase108 manual kernel image relink",
        "phase108 manual kernel image run");
    ExpectTextContainsLinesFile(manual_run_output,
                                ResolveFreestandingKernelGoldenPath(source_root,
                                                                    "phase108_manual_kernel_image.run.contains.txt"),
                                "phase108 manual kernel image run should preserve the landed phase108 slice after target-owned relink");
}

}  // namespace

void RunFreestandingKernelArtifactSuite(const std::filesystem::path& source_root,
                                        const std::filesystem::path& binary_root,
                                        const std::filesystem::path& mc_path) {
    RunFreestandingKernelPhase88ManualRelinkAudit(source_root, binary_root, mc_path);
    RunFreestandingKernelPhase108ManualRelinkAudit(source_root, binary_root, mc_path);
}

}  // namespace mc::tool_tests