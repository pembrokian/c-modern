#include <array>
#include <filesystem>
#include <string>

#include "tests/support/process_utils.h"
#include "tests/tool/tool_suite_common.h"

namespace mc::tool_tests {

using mc::test_support::ExpectOutputContains;
using mc::test_support::Fail;
using mc::test_support::ReadFile;
using mc::test_support::RunCommandCapture;

namespace {

constexpr std::string_view kRunContainsSuffix = ".run.contains.txt";

struct Shard2PhaseRunSpec {
    std::string_view expected_run_lines_file;
    std::string_view output_stem;
    const char* build_context;
    const char* run_context;
    bool expect_phase108_manifest_slice = false;
};

constexpr std::array<Shard2PhaseRunSpec, 5> kShard2PhaseRunSpecs{{
    {
        .expected_run_lines_file = "phase107_real_user_to_user_capability_transfer.run.contains.txt",
        .output_stem = "kernel_phase107_user_transfer",
        .build_context = "freestanding kernel phase107 user-to-user capability transfer build",
        .run_context = "freestanding kernel phase107 user-to-user capability transfer run",
    },
    {
        .expected_run_lines_file = "phase108_kernel_image_program_cap_audit.run.contains.txt",
        .output_stem = "kernel_phase108_image_program_cap",
        .build_context = "freestanding kernel phase108 image-and-program-cap audit build",
        .run_context = "freestanding kernel phase108 image-and-program-cap audit run",
        .expect_phase108_manifest_slice = true,
    },
    {
        .expected_run_lines_file = "phase109_first_running_kernel_slice_audit.run.contains.txt",
        .output_stem = "kernel_phase109_running_slice",
        .build_context = "freestanding kernel phase109 running-slice audit build",
        .run_context = "freestanding kernel phase109 running-slice audit run",
    },
    {
        .expected_run_lines_file = "phase110_kernel_ownership_split_audit.run.contains.txt",
        .output_stem = "kernel_phase110_ownership_split",
        .build_context = "freestanding kernel phase110 ownership-split audit build",
        .run_context = "freestanding kernel phase110 ownership-split audit run",
    },
    {
        .expected_run_lines_file = "phase111_scheduler_lifecycle_ownership_clarification.run.contains.txt",
        .output_stem = "kernel_phase111_lifecycle_ownership",
        .build_context = "freestanding kernel phase111 lifecycle-ownership audit build",
        .run_context = "freestanding kernel phase111 lifecycle-ownership audit run",
    },
}};

bool RunFileMatchesCaseName(std::string_view expected_run_lines_file, std::string_view case_name) {
    if (!expected_run_lines_file.ends_with(kRunContainsSuffix)) {
        return false;
    }
    return expected_run_lines_file.substr(0, expected_run_lines_file.size() - kRunContainsSuffix.size()) == case_name;
}

const Shard2PhaseRunSpec* FindShard2RunSpec(std::string_view case_name) {
    for (const auto& run_spec : kShard2PhaseRunSpecs) {
        if (RunFileMatchesCaseName(run_spec.expected_run_lines_file, case_name)) {
            return &run_spec;
        }
    }
    return nullptr;
}

const FreestandingKernelRuntimePhaseDescriptor& FindShard2PhaseCheck(
    const std::vector<FreestandingKernelRuntimePhaseDescriptor>& phase_checks,
                                                 std::string_view expected_run_lines_file) {
    for (const auto& phase_check : phase_checks) {
        if (phase_check.expected_run_lines_file == expected_run_lines_file) {
            return phase_check;
        }
    }
    Fail("missing shard2 runtime phase entry for run golden: " + std::string(expected_run_lines_file));
}

void ExpectPhase108ManifestSlice(const std::filesystem::path& manifest_path,
                                 const std::filesystem::path& expected_lines_path) {
    ExpectTextContainsLinesFile(ReadFile(manifest_path),
                                expected_lines_path,
                                "phase108 kernel manifest should preserve the executable-owned target contract");
}

void RunShard2Phase(const std::filesystem::path& source_root,
                    const std::filesystem::path& binary_root,
                    const std::filesystem::path& mc_path,
                    const Shard2PhaseRunSpec& run_spec) {
    const auto phase_checks = LoadFreestandingKernelRuntimePhaseDescriptors(source_root, 2);
    const auto& phase_check = FindShard2PhaseCheck(phase_checks, run_spec.expected_run_lines_file);
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);

    if (run_spec.expect_phase108_manifest_slice) {
        const std::filesystem::path manifest_lines_path = ResolveFreestandingKernelGoldenPath(source_root,
                                                                                              "phase108_kernel_manifest.contains.txt");
        ExpectPhase108ManifestSlice(common_paths.project_path, manifest_lines_path);
    }

    const auto artifacts = BuildAndRunFreestandingKernelTarget(mc_path,
                                                               common_paths.project_path,
                                                               common_paths.main_source_path,
                                                               binary_root / "kernel_build",
                                                               run_spec.output_stem,
                                                               run_spec.build_context,
                                                               run_spec.run_context);
    const std::string kernel_mir = ReadFile(artifacts.dump_targets.mir);
    ExpectFreestandingKernelPhaseFromArtifacts(source_root,
                                               artifacts.build_targets.object.parent_path(),
                                               artifacts.run_output,
                                               kernel_mir,
                                               phase_check.View());
}

}  // namespace

void RunFreestandingKernelPhase107RealUserToUserCapabilityTransfer(const std::filesystem::path& source_root,
                                                                   const std::filesystem::path& binary_root,
                                                                   const std::filesystem::path& mc_path) {
    RunShard2Phase(source_root, binary_root, mc_path, kShard2PhaseRunSpecs[0]);
}

void RunFreestandingKernelPhase108KernelImageProgramCapAudit(const std::filesystem::path& source_root,
                                                             const std::filesystem::path& binary_root,
                                                             const std::filesystem::path& mc_path) {
    RunShard2Phase(source_root, binary_root, mc_path, kShard2PhaseRunSpecs[1]);
}

void RunFreestandingKernelPhase109FirstRunningKernelSliceAudit(const std::filesystem::path& source_root,
                                                               const std::filesystem::path& binary_root,
                                                               const std::filesystem::path& mc_path) {
    RunShard2Phase(source_root, binary_root, mc_path, kShard2PhaseRunSpecs[2]);
}

void RunFreestandingKernelPhase110KernelOwnershipSplitAudit(const std::filesystem::path& source_root,
                                                            const std::filesystem::path& binary_root,
                                                            const std::filesystem::path& mc_path) {
    RunShard2Phase(source_root, binary_root, mc_path, kShard2PhaseRunSpecs[3]);
}

void RunFreestandingKernelPhase111SchedulerLifecycleOwnershipClarification(const std::filesystem::path& source_root,
                                                                           const std::filesystem::path& binary_root,
                                                                           const std::filesystem::path& mc_path) {
    RunShard2Phase(source_root, binary_root, mc_path, kShard2PhaseRunSpecs[4]);
}

bool TryRunFreestandingKernelShard2Case(const std::filesystem::path& source_root,
                                        const std::filesystem::path& binary_root,
                                        const std::filesystem::path& mc_path,
                                        std::string_view case_name) {
    const Shard2PhaseRunSpec* run_spec = FindShard2RunSpec(case_name);
    if (run_spec == nullptr) {
        return false;
    }
    RunShard2Phase(source_root, binary_root, mc_path, *run_spec);
    return true;
}

void RunFreestandingKernelShard2(const std::filesystem::path& source_root,
                                 const std::filesystem::path& binary_root,
                                 const std::filesystem::path& mc_path) {
    const auto phase_checks = LoadFreestandingKernelRuntimePhaseDescriptors(source_root, 2);
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path shard_run_lines_path = ResolveFreestandingKernelGoldenPath(source_root,
                                                                        "kernel_shard2.run.contains.txt");
    const std::filesystem::path manifest_lines_path = ResolveFreestandingKernelGoldenPath(source_root,
                                                                       "phase108_kernel_manifest.contains.txt");
    const auto artifacts = BuildAndRunFreestandingKernelTarget(mc_path,
                                                               common_paths.project_path,
                                                               common_paths.main_source_path,
                                                               binary_root / "kernel_build",
                                                               "kernel_shard2",
                                                               "freestanding kernel shard2 build",
                                                               "freestanding kernel shard2 run");

    ExpectTextContainsLinesFile(artifacts.run_output,
                                shard_run_lines_path,
                                "shard2 freestanding kernel run should preserve the landed shard2 slices");
    ExpectPhase108ManifestSlice(common_paths.project_path, manifest_lines_path);
    const std::string kernel_mir = ReadFile(artifacts.dump_targets.mir);
    for (const auto& phase_check : phase_checks) {
        ExpectFreestandingKernelPhaseFromArtifacts(source_root,
                                                   artifacts.build_targets.object.parent_path(),
                                                   artifacts.run_output,
                                                   kernel_mir,
                                                   phase_check.View());
    }
}

}  // namespace mc::tool_tests
