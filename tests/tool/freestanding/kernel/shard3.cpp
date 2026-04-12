#include <filesystem>
#include <string>

#include "tests/support/process_utils.h"
#include "tests/tool/tool_suite_common.h"

namespace mc::tool_tests {

using mc::test_support::ExpectOutputContains;
using mc::test_support::Fail;
using mc::test_support::ReadFile;

namespace {

constexpr std::string_view kRunContainsSuffix = ".run.contains.txt";

struct Shard3PhaseRunSpec {
    std::string_view expected_run_lines_file;
    std::string_view output_stem;
    const char* build_context;
    const char* run_context;
    std::string_view phase_doc_name;
    std::string_view phase_doc_contains;
    const char* phase_doc_context = nullptr;
};

constexpr std::array<Shard3PhaseRunSpec, 5> kShard3PhaseRunSpecs{{
    {
        .expected_run_lines_file = "phase112_syscall_boundary_thinness_audit.run.contains.txt",
        .output_stem = "kernel_phase112_syscall_boundary",
        .build_context = "freestanding kernel phase112 syscall-boundary audit build",
        .run_context = "freestanding kernel phase112 syscall-boundary audit run",
    },
    {
        .expected_run_lines_file = "phase113_interrupt_entry_and_generic_dispatch_boundary.run.contains.txt",
        .output_stem = "kernel_phase113_interrupt_boundary",
        .build_context = "freestanding kernel phase113 interrupt boundary build",
        .run_context = "freestanding kernel phase113 interrupt boundary run",
    },
    {
        .expected_run_lines_file = "phase114_address_space_and_mmu_ownership_split.run.contains.txt",
        .output_stem = "kernel_phase114_address_space_mmu",
        .build_context = "freestanding kernel phase114 address-space/mmu build",
        .run_context = "freestanding kernel phase114 address-space/mmu run",
        .phase_doc_name = "phase114_address_space_and_mmu_ownership_split.txt",
        .phase_doc_contains = "Phase 114 -- Address-Space And MMU Ownership Split",
        .phase_doc_context = "phase114 plan note should exist in canonical phase-doc style",
    },
    {
        .expected_run_lines_file = "phase115_timer_ownership_hardening.run.contains.txt",
        .output_stem = "kernel_phase115_timer_ownership",
        .build_context = "freestanding kernel phase115 timer-ownership build",
        .run_context = "freestanding kernel phase115 timer-ownership run",
    },
    {
        .expected_run_lines_file = "phase116_mmu_activation_barrier_follow_through.run.contains.txt",
        .output_stem = "kernel_phase116_mmu_barrier",
        .build_context = "freestanding kernel phase116 mmu barrier build",
        .run_context = "freestanding kernel phase116 mmu barrier run",
    },
}};

bool RunFileMatchesCaseName(std::string_view expected_run_lines_file, std::string_view case_name) {
    if (!expected_run_lines_file.ends_with(kRunContainsSuffix)) {
        return false;
    }
    return expected_run_lines_file.substr(0, expected_run_lines_file.size() - kRunContainsSuffix.size()) == case_name;
}

const Shard3PhaseRunSpec* FindShard3RunSpec(std::string_view case_name) {
    for (const auto& run_spec : kShard3PhaseRunSpecs) {
        if (RunFileMatchesCaseName(run_spec.expected_run_lines_file, case_name)) {
            return &run_spec;
        }
    }
    return nullptr;
}

const FreestandingKernelRuntimePhaseDescriptor& FindShard3PhaseCheck(
    const std::vector<FreestandingKernelRuntimePhaseDescriptor>& phase_checks,
                                                 std::string_view expected_run_lines_file) {
    for (const auto& phase_check : phase_checks) {
        if (phase_check.expected_run_lines_file == expected_run_lines_file) {
            return phase_check;
        }
    }
    Fail("missing shard3 runtime phase entry for run golden: " + std::string(expected_run_lines_file));
}

void RunShard3Phase(const std::filesystem::path& source_root,
                    const std::filesystem::path& binary_root,
                    const std::filesystem::path& mc_path,
                    const Shard3PhaseRunSpec& run_spec) {
    const auto phase_checks = LoadFreestandingKernelRuntimePhaseDescriptors(source_root, 3);
    const auto& phase_check = FindShard3PhaseCheck(phase_checks, run_spec.expected_run_lines_file);

    if (!run_spec.phase_doc_name.empty()) {
        const std::string phase_doc = ReadFile(ResolvePlanDocPath(source_root, run_spec.phase_doc_name));
        ExpectOutputContains(phase_doc,
                             run_spec.phase_doc_contains,
                             run_spec.phase_doc_context == nullptr ? "freestanding kernel phase doc should preserve the landed slice" : run_spec.phase_doc_context);
    }

    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
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

void RunFreestandingKernelPhase112SyscallBoundaryThinnessAudit(const std::filesystem::path& source_root,
                                                               const std::filesystem::path& binary_root,
                                                               const std::filesystem::path& mc_path) {
    RunShard3Phase(source_root, binary_root, mc_path, kShard3PhaseRunSpecs[0]);
}

void RunFreestandingKernelPhase113InterruptEntryAndGenericDispatchBoundary(const std::filesystem::path& source_root,
                                                                           const std::filesystem::path& binary_root,
                                                                           const std::filesystem::path& mc_path) {
    RunShard3Phase(source_root, binary_root, mc_path, kShard3PhaseRunSpecs[1]);
}

void RunFreestandingKernelPhase114AddressSpaceAndMmuOwnershipSplit(const std::filesystem::path& source_root,
                                                                   const std::filesystem::path& binary_root,
                                                                   const std::filesystem::path& mc_path) {
    RunShard3Phase(source_root, binary_root, mc_path, kShard3PhaseRunSpecs[2]);
}

void RunFreestandingKernelPhase115TimerOwnershipHardening(const std::filesystem::path& source_root,
                                                          const std::filesystem::path& binary_root,
                                                          const std::filesystem::path& mc_path) {
    RunShard3Phase(source_root, binary_root, mc_path, kShard3PhaseRunSpecs[3]);
}

void RunFreestandingKernelPhase116MmuActivationBarrierFollowThrough(const std::filesystem::path& source_root,
                                                                    const std::filesystem::path& binary_root,
                                                                    const std::filesystem::path& mc_path) {
    RunShard3Phase(source_root, binary_root, mc_path, kShard3PhaseRunSpecs[4]);
}

bool TryRunFreestandingKernelShard3Case(const std::filesystem::path& source_root,
                                        const std::filesystem::path& binary_root,
                                        const std::filesystem::path& mc_path,
                                        std::string_view case_name) {
    const Shard3PhaseRunSpec* run_spec = FindShard3RunSpec(case_name);
    if (run_spec == nullptr) {
        return false;
    }
    RunShard3Phase(source_root, binary_root, mc_path, *run_spec);
    return true;
}

void RunFreestandingKernelShard3(const std::filesystem::path& source_root,
                                 const std::filesystem::path& binary_root,
                                 const std::filesystem::path& mc_path) {
    const auto phase_checks = LoadFreestandingKernelRuntimePhaseDescriptors(source_root, 3);
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const auto artifacts = BuildAndRunFreestandingKernelTarget(mc_path,
                                                               common_paths.project_path,
                                                               common_paths.main_source_path,
                                                               binary_root / "kernel_build",
                                                               "kernel_shard3",
                                                               "freestanding kernel shard3 build",
                                                               "freestanding kernel shard3 run");

    ExpectTextContainsLinesFile(artifacts.run_output,
                                ResolveFreestandingKernelGoldenPath(source_root, "kernel_shard3.run.contains.txt"),
                                "shard3 freestanding kernel run should preserve the landed shard3 slices");

    const std::string kernel_mir = ReadFile(artifacts.dump_targets.mir);
    const std::filesystem::path object_dir = artifacts.build_targets.object.parent_path();
    for (const auto& phase_check : phase_checks) {
        ExpectFreestandingKernelPhaseFromArtifacts(source_root,
                                                   object_dir,
                                                   artifacts.run_output,
                                                   kernel_mir,
                                                   phase_check.View());
    }
}

}  // namespace mc::tool_tests
