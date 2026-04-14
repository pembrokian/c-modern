#include <filesystem>
#include <string>
#include <vector>

#include "compiler/support/dump_paths.h"
#include "compiler/support/target.h"
#include "tests/support/process_utils.h"
#include "tests/tool/tool_suite_common.h"

namespace mc::tool_tests {

using mc::test_support::Fail;
using mc::test_support::ReadFile;
using mc::test_support::WriteFile;

namespace {

std::filesystem::path ResolveArtifactSpecFile(const FreestandingKernelArtifactAuditDescriptor& descriptor,
                                              std::string_view file_name) {
    return descriptor.descriptor_dir / std::string(file_name);
}

void RunStandaloneManualRelinkAudit(const FreestandingKernelArtifactAuditDescriptor& descriptor,
                                    const std::filesystem::path& source_root,
                                    const std::filesystem::path& binary_root,
                                    const std::filesystem::path& mc_path) {
    const std::filesystem::path project_root = binary_root / (descriptor.label + "_project");
    std::filesystem::remove_all(project_root);

    WriteFile(project_root / "src/main.mc", ReadFile(ResolveArtifactSpecFile(descriptor, descriptor.main_source_file)));
    WriteFile(project_root / "target/driver_support.c",
              ReadFile(ResolveArtifactSpecFile(descriptor, descriptor.driver_support_source_file)));

    const std::filesystem::path driver_support_object = project_root / "target/driver_support.o";
    CompileBootstrapCObjectAndExpectSuccess(project_root / "target/driver_support.c",
                                            driver_support_object,
                                            project_root / "target" / (descriptor.label + "_driver_support_compile_output.txt"),
                                            descriptor.label + " artifact descriptor driver support object compile");

    WriteFile(project_root / "build.toml",
              "schema = 1\n"
              "project = \"" + descriptor.project_name + "\"\n"
              "default = \"kernel\"\n"
              "\n"
              "[targets.kernel]\n"
              "kind = \"exe\"\n"
              "package = \"kernel\"\n"
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
              "inputs = [\"target/driver_support.o\"]\n");

    const std::filesystem::path project_path = project_root / "build.toml";
    const std::filesystem::path build_dir = binary_root / (descriptor.label + "_build");
    MaybeCleanBuildDir(build_dir);

    BuildProjectTargetAndExpectSuccess(mc_path,
                                       project_path,
                                       build_dir,
                                       "kernel",
                                       descriptor.label + "_build_output.txt",
                                       descriptor.label + " artifact descriptor freestanding kernel build integration build");

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(project_root / "src/main.mc", build_dir);
    const std::filesystem::path runtime_object =
        build_targets.object.parent_path() /
        (build_targets.object.stem().generic_string() + ".freestanding.bootstrap_main.runtime.o");
    if (!std::filesystem::exists(build_targets.object)) {
        Fail(descriptor.label + " artifact descriptor should emit an entry object artifact");
    }
    if (!std::filesystem::exists(runtime_object)) {
        Fail(descriptor.label + " artifact descriptor should emit a freestanding runtime object artifact");
    }

    WriteFile(project_root / "target/manual_support.c",
              ReadFile(ResolveArtifactSpecFile(descriptor, descriptor.manual_support_source_file)));
    const std::filesystem::path manual_support_object = project_root / "target/manual_support.o";
    CompileBootstrapCObjectAndExpectSuccess(project_root / "target/manual_support.c",
                                            manual_support_object,
                                            project_root / "target" / (descriptor.label + "_manual_support_compile_output.txt"),
                                            descriptor.label + " artifact descriptor manual support object compile");

    const std::filesystem::path manual_executable = build_dir / "bin" / (descriptor.label + "_manual_kernel");
    LinkBootstrapObjectsAndRunExpectExitCode({build_targets.object, manual_support_object, runtime_object},
                                             manual_executable,
                                             build_dir / (descriptor.label + "_manual_link_output.txt"),
                                             build_dir / (descriptor.label + "_manual_link_run_output.txt"),
                                             descriptor.expected_exit_code,
                                             descriptor.label + " manual freestanding kernel link",
                                             descriptor.label + " manual freestanding kernel run");
}

void RunKernelManualRelinkAudit(const FreestandingKernelArtifactAuditDescriptor& descriptor,
                                const std::filesystem::path& source_root,
                                const std::filesystem::path& binary_root,
                                const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const auto artifacts = BuildAndRunFreestandingKernelTarget(mc_path,
                                                               common_paths.project_path,
                                                               common_paths.main_source_path,
                                                               binary_root / (descriptor.label + "_build"),
                                                               descriptor.output_stem,
                                                               descriptor.build_context,
                                                               descriptor.run_context);

    const std::filesystem::path runtime_object =
        artifacts.build_targets.object.parent_path() /
        (artifacts.build_targets.object.stem().generic_string() + ".freestanding.bootstrap_main.runtime.o");
    if (!std::filesystem::exists(artifacts.build_targets.object)) {
        Fail(descriptor.label + " should emit an entry object artifact for target-owned relink");
    }
    if (!std::filesystem::exists(runtime_object)) {
        Fail(descriptor.label + " should emit a freestanding runtime object artifact for target-owned relink");
    }
    if (!std::filesystem::exists(artifacts.build_targets.executable)) {
        Fail(descriptor.label + " should emit the linked kernel executable artifact");
    }

    const std::filesystem::path manual_executable = artifacts.build_dir / "bin" / (descriptor.label + "_manual_kernel_image");
    const std::string manual_run_output = LinkBootstrapObjectsAndRunExpectExitCode(
        CollectBootstrapObjectFiles(artifacts.build_targets.object.parent_path()),
        manual_executable,
        artifacts.build_dir / (descriptor.label + "_manual_link_output.txt"),
        artifacts.build_dir / (descriptor.label + "_manual_link_run_output.txt"),
        0,
        descriptor.label + " manual kernel image relink",
        descriptor.label + " manual kernel image run");
    ExpectTextContainsLinesFile(manual_run_output,
                                ResolveFreestandingKernelGoldenPath(source_root, descriptor.expected_run_lines_file),
                                descriptor.label + " manual kernel image run should preserve the landed runtime slice after target-owned relink");
}

void RunFileContainsArtifactAudit(const FreestandingKernelArtifactAuditDescriptor& descriptor,
                                  const std::filesystem::path& source_root) {
    ExpectTextContainsLinesFile(ReadFile(source_root / descriptor.source_file),
                                ResolveFreestandingKernelGoldenPath(source_root, descriptor.expected_run_lines_file),
                                descriptor.label + " file contains audit");
}

}  // namespace

void RunFreestandingKernelArtifactSuite(const std::filesystem::path& source_root,
                                        const std::filesystem::path& binary_root,
                                        const std::filesystem::path& mc_path) {
    for (const auto& descriptor : LoadFreestandingKernelArtifactAuditDescriptors(source_root)) {
        if (descriptor.kind == "standalone_manual_relink") {
            RunStandaloneManualRelinkAudit(descriptor, source_root, binary_root, mc_path);
            continue;
        }
        if (descriptor.kind == "kernel_manual_relink") {
            RunKernelManualRelinkAudit(descriptor, source_root, binary_root, mc_path);
            continue;
        }
        if (descriptor.kind == "file_contains") {
            RunFileContainsArtifactAudit(descriptor, source_root);
            continue;
        }
        Fail("unsupported kernel artifact audit kind: " + descriptor.kind);
    }
}

}  // namespace mc::tool_tests