#include <filesystem>
#include <string>

#include "compiler/support/dump_paths.h"
#include "tests/support/process_utils.h"
#include "tests/tool/tool_suite_common.h"

namespace mc::tool_tests {

using mc::test_support::ExpectOutputContains;
using mc::test_support::Fail;
using mc::test_support::ReadFile;
using mc::test_support::RunCommandCapture;

namespace {

void ExpectPhase112BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets,
                                 const std::filesystem::path& expected_lines_path) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase112_syscall_boundary_run_output.txt",
                                                             "freestanding kernel phase112 syscall-boundary audit run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("phase112 freestanding kernel syscall-boundary audit run should preserve the landed phase112 slice\n" + run_output);
    }
    ExpectTextContainsLinesFile(run_output,
                                expected_lines_path,
                                "phase112 freestanding kernel syscall-boundary audit run should preserve the landed phase112 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "kernel__capability.o")) {
        Fail("phase112 syscall boundary audit should emit the capability module object");
    }
    if (!std::filesystem::exists(object_dir / "kernel__ipc.o")) {
        Fail("phase112 syscall boundary audit should emit the ipc module object");
    }
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_address_space.mc.o")) {
        Fail("phase112 syscall boundary audit should emit the address-space module object");
    }
}

void ExpectPhase112MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "Function name=address_space.build_child_bootstrap_context returns=[address_space.SpawnBootstrap]",
            "Function name=address_space.validate_syscall_address_space_boundary returns=[bool]",
            "Function name=capability.resolve_endpoint_handle returns=[capability.EndpointHandleResolution]",
            "Function name=capability.resolve_attached_transfer_handle returns=[capability.AttachedTransferResolution]",
            "Function name=capability.install_received_endpoint_handle returns=[capability.ReceivedHandleInstall]",
            "Function name=capability.validate_syscall_capability_boundary returns=[bool]",
            "Function name=ipc.build_runtime_message returns=[ipc.KernelMessage]",
            "Function name=ipc.enqueue_runtime_message returns=[ipc.RuntimeSendResult]",
            "Function name=ipc.receive_runtime_message returns=[ipc.RuntimeReceiveResult]",
            "Function name=ipc.validate_syscall_ipc_boundary returns=[bool]",
            "Function name=debug.validate_phase112_syscall_boundary_thinness returns=[bool]",
        },
        expected_projection_path,
        "phase112 merged MIR should preserve the syscall boundary projection");
}


void ExpectPhase113BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets,
                                 const std::filesystem::path& expected_lines_path) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase113_interrupt_boundary_run_output.txt",
                                                             "freestanding kernel phase113 interrupt boundary run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("phase113 freestanding kernel interrupt boundary run should preserve the landed phase113 slice\n" + run_output);
    }
    ExpectTextContainsLinesFile(run_output,
                                expected_lines_path,
                                "phase113 freestanding kernel interrupt boundary run should preserve the landed phase113 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_interrupt.mc.o")) {
        Fail("phase113 interrupt boundary audit should emit the interrupt module object");
    }
}

void ExpectPhase113MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "Function name=interrupt.arch_enter_interrupt returns=[interrupt.InterruptEntry]",
            "Function name=interrupt.dispatch_interrupt returns=[interrupt.InterruptDispatchResult]",
            "Function name=interrupt.validate_interrupt_entry_and_dispatch_boundary returns=[bool]",
            "Function name=debug.validate_phase113_interrupt_entry_and_generic_dispatch_boundary returns=[bool]",
        },
        expected_projection_path,
        "phase113 merged MIR should preserve the interrupt boundary projection");
}


void ExpectPhase114BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets,
                                 const std::filesystem::path& expected_lines_path) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase114_address_space_mmu_run_output.txt",
                                                             "freestanding kernel phase114 address-space/mmu run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("phase114 freestanding kernel address-space/mmu run should preserve the landed phase114 slice\n" + run_output);
    }
    ExpectTextContainsLinesFile(run_output,
                                expected_lines_path,
                                "phase114 freestanding kernel address-space/mmu run should preserve the landed phase114 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_mmu.mc.o")) {
        Fail("phase114 address-space/mmu audit should emit the mmu module object");
    }
}

void ExpectPhase114MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "TypeDecl kind=struct name=mmu.TranslationRoot",
            "Function name=mmu.bootstrap_translation_root returns=[mmu.TranslationRoot]",
            "Function name=mmu.validate_address_space_mmu_boundary returns=[bool]",
            "Function name=debug.validate_phase114_address_space_and_mmu_ownership_split returns=[bool]",
        },
        expected_projection_path,
        "phase114 merged MIR should preserve the address-space/mmu boundary projection");
}


void ExpectPhase115BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets,
                                 const std::filesystem::path& expected_lines_path) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase115_timer_ownership_run_output.txt",
                                                             "freestanding kernel phase115 timer-ownership run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("phase115 freestanding kernel timer-ownership run should preserve the landed phase115 slice\n" + run_output);
    }
    ExpectTextContainsLinesFile(run_output,
                                expected_lines_path,
                                "phase115 freestanding kernel timer-ownership run should preserve the landed phase115 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_timer.mc.o")) {
        Fail("phase115 timer-ownership audit should emit the timer module object");
    }
}

void ExpectPhase115MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "TypeDecl kind=struct name=timer.TimerInterruptDelivery",
            "Function name=timer.deliver_interrupt_tick returns=[timer.TimerInterruptDelivery]",
            "Function name=timer.validate_interrupt_delivery_boundary returns=[bool]",
            "Function name=debug.validate_phase115_timer_ownership_hardening returns=[bool]",
        },
        expected_projection_path,
        "phase115 merged MIR should preserve the timer-ownership projection");
}


void ExpectPhase116BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets,
                                 const std::filesystem::path& expected_lines_path) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase116_mmu_barrier_run_output.txt",
                                                             "freestanding kernel phase116 mmu barrier run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("phase116 freestanding kernel mmu barrier run should preserve the landed phase116 slice\n" + run_output);
    }
    ExpectTextContainsLinesFile(run_output,
                                expected_lines_path,
                                "phase116 freestanding kernel mmu barrier run should preserve the landed phase116 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_mmu.mc.o")) {
        Fail("phase116 mmu barrier audit should emit the mmu module object");
    }
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_stdlib_hal.mc.o")) {
        Fail("phase116 mmu barrier audit should emit the hal module object");
    }
}

void ExpectPhase116MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "TypeDecl kind=struct name=mmu.ActivationBarrierObservation",
            "Function name=hal.memory_barrier",
            "Function name=mmu.activate_with_barrier returns=[mmu.ActivationBarrierObservation]",
            "Function name=mmu.validate_activation_barrier_boundary returns=[bool]",
            "Function name=debug.validate_phase116_mmu_activation_barrier_follow_through returns=[bool]",
        },
        expected_projection_path,
        "phase116 merged MIR should preserve the MMU barrier projection");
}


}  // namespace

void RunFreestandingKernelPhase112SyscallBoundaryThinnessAudit(const std::filesystem::path& source_root,
                                                               const std::filesystem::path& binary_root,
                                                               const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase112_syscall_boundary_thinness_audit.mirproj.txt";
    const std::filesystem::path build_dir = binary_root / "kernel_build";
    MaybeCleanBuildDir(build_dir);

    const auto [build_outcome, build_output] = RunCommandCapture({mc_path.generic_string(),
                                                                  "build",
                                                                  "--project",
                                                                  common_paths.project_path.generic_string(),
                                                                  "--target",
                                                                  "kernel",
                                                                  "--build-dir",
                                                                  build_dir.generic_string(),
                                                                  "--dump-mir"},
                                                                 build_dir / "kernel_phase112_syscall_boundary_build_output.txt",
                                                                 "freestanding kernel phase112 syscall-boundary audit build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase112 freestanding kernel syscall-boundary audit build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase112BehaviorSlice(build_dir,
                                build_targets,
                                ResolveFreestandingKernelGoldenPath(source_root,
                                                                    "phase112_syscall_boundary_thinness_audit.run.contains.txt"));
    ExpectPhase112MirStructureSlice(dump_targets.mir, mir_projection_path);
}

void RunFreestandingKernelPhase113InterruptEntryAndGenericDispatchBoundary(const std::filesystem::path& source_root,
                                                                           const std::filesystem::path& binary_root,
                                                                           const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase113_interrupt_entry_and_generic_dispatch_boundary.mirproj.txt";
    const std::filesystem::path build_dir = binary_root / "kernel_build";
    MaybeCleanBuildDir(build_dir);

    const auto [build_outcome, build_output] = RunCommandCapture({mc_path.generic_string(),
                                                                  "build",
                                                                  "--project",
                                                                  common_paths.project_path.generic_string(),
                                                                  "--target",
                                                                  "kernel",
                                                                  "--build-dir",
                                                                  build_dir.generic_string(),
                                                                  "--dump-mir"},
                                                                 build_dir / "kernel_phase113_interrupt_boundary_build_output.txt",
                                                                 "freestanding kernel phase113 interrupt boundary build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase113 freestanding kernel interrupt boundary build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase113BehaviorSlice(build_dir,
                                build_targets,
                                ResolveFreestandingKernelGoldenPath(source_root,
                                                                    "phase113_interrupt_entry_and_generic_dispatch_boundary.run.contains.txt"));
    ExpectPhase113MirStructureSlice(dump_targets.mir, mir_projection_path);
}

void RunFreestandingKernelPhase114AddressSpaceAndMmuOwnershipSplit(const std::filesystem::path& source_root,
                                                                   const std::filesystem::path& binary_root,
                                                                   const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path build_dir = binary_root / "kernel_build";
    const std::filesystem::path phase_doc_path = ResolvePlanDocPath(source_root,
                                                                    "phase114_address_space_and_mmu_ownership_split.txt");
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase114_address_space_and_mmu_ownership_split.mirproj.txt";
    MaybeCleanBuildDir(build_dir);

    const auto [build_outcome, build_output] = RunCommandCapture({mc_path.generic_string(),
                                                                  "build",
                                                                  "--project",
                                                                  common_paths.project_path.generic_string(),
                                                                  "--target",
                                                                  "kernel",
                                                                  "--build-dir",
                                                                  build_dir.generic_string(),
                                                                  "--dump-mir"},
                                                                 build_dir / "kernel_phase114_address_space_mmu_build_output.txt",
                                                                 "freestanding kernel phase114 address-space/mmu build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase114 freestanding kernel address-space/mmu build should succeed:\n" + build_output);
    }

    const std::string phase_doc = ReadFile(phase_doc_path);
    ExpectOutputContains(phase_doc,
                         "Phase 114 -- Address-Space And MMU Ownership Split",
                         "phase114 plan note should exist in canonical phase-doc style");

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase114BehaviorSlice(build_dir,
                                build_targets,
                                ResolveFreestandingKernelGoldenPath(source_root,
                                                                    "phase114_address_space_and_mmu_ownership_split.run.contains.txt"));
    ExpectPhase114MirStructureSlice(dump_targets.mir, mir_projection_path);
}

void RunFreestandingKernelPhase115TimerOwnershipHardening(const std::filesystem::path& source_root,
                                                          const std::filesystem::path& binary_root,
                                                          const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase115_timer_ownership_hardening.mirproj.txt";
    const std::filesystem::path build_dir = binary_root / "kernel_build";
    MaybeCleanBuildDir(build_dir);

    const auto [build_outcome, build_output] = RunCommandCapture({mc_path.generic_string(),
                                                                  "build",
                                                                  "--project",
                                                                  common_paths.project_path.generic_string(),
                                                                  "--target",
                                                                  "kernel",
                                                                  "--build-dir",
                                                                  build_dir.generic_string(),
                                                                  "--dump-mir"},
                                                                 build_dir / "kernel_phase115_timer_ownership_build_output.txt",
                                                                 "freestanding kernel phase115 timer-ownership build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase115 freestanding kernel timer-ownership build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase115BehaviorSlice(build_dir,
                                build_targets,
                                ResolveFreestandingKernelGoldenPath(source_root,
                                                                    "phase115_timer_ownership_hardening.run.contains.txt"));
    ExpectPhase115MirStructureSlice(dump_targets.mir, mir_projection_path);
}

void RunFreestandingKernelPhase116MmuActivationBarrierFollowThrough(const std::filesystem::path& source_root,
                                                                    const std::filesystem::path& binary_root,
                                                                    const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path freestanding_support_path = source_root / "docs" / "plan" /
                                                            "freestanding_support_statement.txt";
    const std::filesystem::path stdlib_readme_path = source_root / "stdlib" / "README.md";
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase116_mmu_activation_barrier_follow_through.mirproj.txt";
    const std::filesystem::path build_dir = binary_root / "kernel_build";
    MaybeCleanBuildDir(build_dir);

    const auto [build_outcome, build_output] = RunCommandCapture({mc_path.generic_string(),
                                                                  "build",
                                                                  "--project",
                                                                  common_paths.project_path.generic_string(),
                                                                  "--target",
                                                                  "kernel",
                                                                  "--build-dir",
                                                                  build_dir.generic_string(),
                                                                  "--dump-mir"},
                                                                 build_dir / "kernel_phase116_mmu_barrier_build_output.txt",
                                                                 "freestanding kernel phase116 mmu barrier build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase116 freestanding kernel mmu barrier build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase116BehaviorSlice(build_dir,
                                build_targets,
                                ResolveFreestandingKernelGoldenPath(source_root,
                                                                    "phase116_mmu_activation_barrier_follow_through.run.contains.txt"));
    ExpectPhase116MirStructureSlice(dump_targets.mir, mir_projection_path);
}

void RunFreestandingKernelShard3(const std::filesystem::path& source_root,
                                 const std::filesystem::path& binary_root,
                                 const std::filesystem::path& mc_path) {
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
    static constexpr std::string_view kPhase112Objects[] = {
        "kernel__capability.o",
        "kernel__ipc.o",
        "_Users_ro_dev_c_modern_kernel_src_address_space.mc.o",
    };
    static constexpr std::string_view kPhase112Selectors[] = {
        "Function name=address_space.build_child_bootstrap_context returns=[address_space.SpawnBootstrap]",
        "Function name=address_space.validate_syscall_address_space_boundary returns=[bool]",
        "Function name=capability.resolve_endpoint_handle returns=[capability.EndpointHandleResolution]",
        "Function name=capability.resolve_attached_transfer_handle returns=[capability.AttachedTransferResolution]",
        "Function name=capability.install_received_endpoint_handle returns=[capability.ReceivedHandleInstall]",
        "Function name=capability.validate_syscall_capability_boundary returns=[bool]",
        "Function name=ipc.build_runtime_message returns=[ipc.KernelMessage]",
        "Function name=ipc.enqueue_runtime_message returns=[ipc.RuntimeSendResult]",
        "Function name=ipc.receive_runtime_message returns=[ipc.RuntimeReceiveResult]",
        "Function name=ipc.validate_syscall_ipc_boundary returns=[bool]",
        "Function name=debug.validate_phase112_syscall_boundary_thinness returns=[bool]",
    };
    static constexpr std::string_view kPhase113Objects[] = {
        "_Users_ro_dev_c_modern_kernel_src_interrupt.mc.o",
    };
    static constexpr std::string_view kPhase113Selectors[] = {
        "Function name=interrupt.arch_enter_interrupt returns=[interrupt.InterruptEntry]",
        "Function name=interrupt.dispatch_interrupt returns=[interrupt.InterruptDispatchResult]",
        "Function name=interrupt.validate_interrupt_entry_and_dispatch_boundary returns=[bool]",
        "Function name=debug.validate_phase113_interrupt_entry_and_generic_dispatch_boundary returns=[bool]",
    };
    static constexpr std::string_view kPhase114Objects[] = {
        "_Users_ro_dev_c_modern_kernel_src_mmu.mc.o",
    };
    static constexpr std::string_view kPhase114Selectors[] = {
        "TypeDecl kind=struct name=mmu.TranslationRoot",
        "Function name=mmu.bootstrap_translation_root returns=[mmu.TranslationRoot]",
        "Function name=mmu.validate_address_space_mmu_boundary returns=[bool]",
        "Function name=debug.validate_phase114_address_space_and_mmu_ownership_split returns=[bool]",
    };
    static constexpr std::string_view kPhase115Objects[] = {
        "_Users_ro_dev_c_modern_kernel_src_timer.mc.o",
    };
    static constexpr std::string_view kPhase115Selectors[] = {
        "TypeDecl kind=struct name=timer.TimerInterruptDelivery",
        "Function name=timer.deliver_interrupt_tick returns=[timer.TimerInterruptDelivery]",
        "Function name=timer.validate_interrupt_delivery_boundary returns=[bool]",
        "Function name=debug.validate_phase115_timer_ownership_hardening returns=[bool]",
    };
    static constexpr std::string_view kPhase116Objects[] = {
        "_Users_ro_dev_c_modern_kernel_src_mmu.mc.o",
        "_Users_ro_dev_c_modern_stdlib_hal.mc.o",
    };
    static constexpr std::string_view kPhase116Selectors[] = {
        "TypeDecl kind=struct name=mmu.ActivationBarrierObservation",
        "Function name=hal.memory_barrier",
        "Function name=mmu.activate_with_barrier returns=[mmu.ActivationBarrierObservation]",
        "Function name=mmu.validate_activation_barrier_boundary returns=[bool]",
        "Function name=debug.validate_phase116_mmu_activation_barrier_follow_through returns=[bool]",
    };

    const FreestandingKernelPhaseCheck phase_checks[] = {
        {"phase112 syscall boundary audit", "phase112_syscall_boundary_thinness_audit.run.contains.txt", std::span{kPhase112Objects}, std::span{kPhase112Selectors}, "phase112_syscall_boundary_thinness_audit.mirproj.txt"},
        {"phase113 interrupt boundary audit", "phase113_interrupt_entry_and_generic_dispatch_boundary.run.contains.txt", std::span{kPhase113Objects}, std::span{kPhase113Selectors}, "phase113_interrupt_entry_and_generic_dispatch_boundary.mirproj.txt"},
        {"phase114 address-space/mmu audit", "phase114_address_space_and_mmu_ownership_split.run.contains.txt", std::span{kPhase114Objects}, std::span{kPhase114Selectors}, "phase114_address_space_and_mmu_ownership_split.mirproj.txt"},
        {"phase115 timer-ownership audit", "phase115_timer_ownership_hardening.run.contains.txt", std::span{kPhase115Objects}, std::span{kPhase115Selectors}, "phase115_timer_ownership_hardening.mirproj.txt"},
        {"phase116 mmu barrier audit", "phase116_mmu_activation_barrier_follow_through.run.contains.txt", std::span{kPhase116Objects}, std::span{kPhase116Selectors}, "phase116_mmu_activation_barrier_follow_through.mirproj.txt"},
    };

    const std::string kernel_mir = ReadFile(artifacts.dump_targets.mir);
    const std::filesystem::path object_dir = artifacts.build_targets.object.parent_path();
    for (const auto& phase_check : phase_checks) {
        ExpectFreestandingKernelPhaseFromArtifacts(source_root,
                                                  object_dir,
                                                  artifacts.run_output,
                                                  kernel_mir,
                                                  phase_check);
    }
}

}  // namespace mc::tool_tests
