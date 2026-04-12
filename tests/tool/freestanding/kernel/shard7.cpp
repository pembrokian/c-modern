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

void ExpectPhase133BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets,
                                 const std::filesystem::path& expected_lines_path) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase133_message_lifetime_run_output.txt",
                                                             "freestanding kernel phase133 message lifetime and reuse run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("phase133 freestanding kernel message lifetime and reuse run should preserve the landed phase133 slice\n" + run_output);
    }
    ExpectTextContainsLinesFile(run_output,
                                expected_lines_path,
                                "phase133 freestanding kernel message lifetime and reuse run should preserve the landed phase133 slice");
}

void ExpectPhase133MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "TypeDecl kind=struct name=debug.Phase133MessageLifetimeAudit",
            "Function name=ipc.close_runtime_endpoint returns=[ipc.EndpointCloseTransition]",
            "Function name=ipc.close_runtime_endpoints_for_owner returns=[ipc.EndpointOwnerCloseTransition]",
            "Function name=debug.validate_phase133_message_lifetime_and_reuse returns=[bool]",
        },
        expected_projection_path,
        "phase133 merged MIR should preserve the message-lifetime projection");
}


void ExpectPhase134BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets,
                                 const std::filesystem::path& expected_lines_path) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase134_minimal_device_service_handoff_run_output.txt",
                                                             "freestanding kernel phase134 minimal device-service handoff run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("phase134 freestanding kernel minimal device-service handoff run should preserve the landed phase134 slice\n" + run_output);
    }
    ExpectTextContainsLinesFile(run_output,
                                expected_lines_path,
                                "phase134 freestanding kernel minimal device-service handoff run should preserve the landed phase134 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_uart.mc.o")) {
        Fail("phase134 minimal device-service handoff should emit the uart module object");
    }
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_serial_service.mc.o")) {
        Fail("phase134 minimal device-service handoff should emit the serial_service module object");
    }
}

void ExpectPhase134MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "TypeDecl kind=struct name=debug.Phase134MinimalDeviceServiceHandoffAudit",
            "Function name=ipc.publish_runtime_byte returns=[ipc.RuntimePublishResult]",
            "Function name=uart.handle_receive_interrupt returns=[uart.UartInterruptResult]",
            "Function name=bootstrap_services.execute_serial_service_receive_and_exit returns=[bootstrap_services.SerialServiceExecutionResult]",
            "Function name=debug.validate_phase134_minimal_device_service_handoff returns=[bool]",
        },
        expected_projection_path,
        "phase134 merged MIR should preserve the minimal device-service handoff projection");
}


void ExpectPhase135BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets,
                                 const std::filesystem::path& expected_lines_path) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase135_buffer_ownership_boundary_run_output.txt",
                                                             "freestanding kernel phase135 buffer ownership boundary run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("phase135 freestanding kernel buffer ownership boundary run should preserve the landed phase135 slice\n" + run_output);
    }
    ExpectTextContainsLinesFile(run_output,
                                expected_lines_path,
                                "phase135 freestanding kernel buffer ownership boundary run should preserve the landed phase135 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_uart.mc.o")) {
        Fail("phase135 buffer ownership boundary should emit the uart module object");
    }
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_serial_service.mc.o")) {
        Fail("phase135 buffer ownership boundary should emit the serial_service module object");
    }
}

void ExpectPhase135MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "TypeDecl kind=struct name=debug.Phase135BufferOwnershipAudit",
            "Function name=ipc.publish_runtime_frame returns=[ipc.RuntimePublishResult]",
            "Function name=uart.stage_receive_frame returns=[uart.UartDevice]",
            "Function name=bootstrap_services.execute_serial_service_receive returns=[bootstrap_services.SerialServiceExecutionResult]",
            "Function name=debug.validate_phase135_buffer_ownership_boundary returns=[bool]",
        },
        expected_projection_path,
        "phase135 merged MIR should preserve the ownership-boundary projection");
}


void ExpectPhase136BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets,
                                 const std::filesystem::path& expected_lines_path) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase136_device_failure_containment_run_output.txt",
                                                             "freestanding kernel phase136 device failure containment run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("phase136 freestanding kernel device failure containment run should preserve the landed phase136 slice\n" + run_output);
    }
    ExpectTextContainsLinesFile(run_output,
                                expected_lines_path,
                                "phase136 freestanding kernel device failure containment run should preserve the landed phase136 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_uart.mc.o")) {
        Fail("phase136 device failure containment probe should emit the uart module object");
    }
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_serial_service.mc.o")) {
        Fail("phase136 device failure containment probe should emit the serial_service module object");
    }
}

void ExpectPhase136MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "TypeDecl kind=struct name=debug.Phase136DeviceFailureContainmentAudit",
            "Function name=uart.handle_receive_interrupt returns=[uart.UartInterruptResult]",
            "Function name=serial_service.record_ingress returns=[serial_service.SerialServiceState]",
            "Function name=bootstrap_services.close_serial_service_after_failure returns=[bootstrap_services.SerialServiceExecutionResult]",
            "Function name=debug.validate_phase136_device_failure_containment returns=[bool]",
        },
        expected_projection_path,
        "phase136 merged MIR should preserve the containment-boundary projection");
}


void ExpectPhase137BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets,
                                 const std::filesystem::path& expected_lines_path) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase137_optional_dma_follow_through_run_output.txt",
                                                             "freestanding kernel phase137 optional dma-or-equivalent follow-through run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("phase137 freestanding kernel optional dma-or-equivalent follow-through run should preserve the landed phase137 slice\n" + run_output);
    }
    ExpectTextContainsLinesFile(run_output,
                                expected_lines_path,
                                "phase137 freestanding kernel optional dma-or-equivalent follow-through run should preserve the landed phase137 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_uart.mc.o")) {
        Fail("phase137 optional dma-or-equivalent probe should emit the uart module object");
    }
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_interrupt.mc.o")) {
        Fail("phase137 optional dma-or-equivalent probe should emit the interrupt module object");
    }
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_serial_service.mc.o")) {
        Fail("phase137 optional dma-or-equivalent probe should emit the serial_service module object");
    }
}

void ExpectPhase137MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "TypeDecl kind=struct name=debug.Phase137OptionalDmaOrEquivalentAudit",
            "Function name=uart.handle_receive_completion_interrupt returns=[uart.UartCompletionInterruptResult]",
            "Function name=interrupt.dispatch_interrupt returns=[interrupt.InterruptDispatchResult]",
            "Function name=serial_service.record_ingress returns=[serial_service.SerialServiceState]",
            "Function name=debug.validate_phase137_optional_dma_or_equivalent_follow_through returns=[bool]",
        },
        expected_projection_path,
        "phase137 merged MIR should preserve the completion-backed boundary projection");
}


}  // namespace

void RunFreestandingKernelPhase133MessageLifetimeAndReuse(const std::filesystem::path& source_root,
                                                          const std::filesystem::path& binary_root,
                                                          const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase133_message_lifetime_and_reuse.mirproj.txt";
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
                                                                 build_dir / "kernel_phase133_message_lifetime_build_output.txt",
                                                                 "freestanding kernel phase133 message lifetime and reuse build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase133 freestanding kernel message lifetime and reuse build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase133BehaviorSlice(build_dir,
                                build_targets,
                                ResolveFreestandingKernelGoldenPath(source_root,
                                                                    "phase133_message_lifetime_and_reuse.run.contains.txt"));
    ExpectPhase133MirStructureSlice(dump_targets.mir, mir_projection_path);
}

void RunFreestandingKernelPhase134MinimalDeviceServiceHandoff(const std::filesystem::path& source_root,
                                                              const std::filesystem::path& binary_root,
                                                              const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase134_minimal_device_service_handoff.mirproj.txt";
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
                                                                 build_dir / "kernel_phase134_minimal_device_service_handoff_build_output.txt",
                                                                 "freestanding kernel phase134 minimal device-service handoff build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase134 freestanding kernel minimal device-service handoff build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase134BehaviorSlice(build_dir,
                                build_targets,
                                ResolveFreestandingKernelGoldenPath(source_root,
                                                                    "phase134_minimal_device_service_handoff.run.contains.txt"));
    ExpectPhase134MirStructureSlice(dump_targets.mir, mir_projection_path);
}

void RunFreestandingKernelPhase135BufferOwnershipBoundaryAudit(const std::filesystem::path& source_root,
                                                               const std::filesystem::path& binary_root,
                                                               const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase135_buffer_ownership_boundary_audit.mirproj.txt";
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
                                                                 build_dir / "kernel_phase135_buffer_ownership_boundary_build_output.txt",
                                                                 "freestanding kernel phase135 buffer ownership boundary build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase135 freestanding kernel buffer ownership boundary build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase135BehaviorSlice(build_dir,
                                build_targets,
                                ResolveFreestandingKernelGoldenPath(source_root,
                                                                    "phase135_buffer_ownership_boundary_audit.run.contains.txt"));
    ExpectPhase135MirStructureSlice(dump_targets.mir, mir_projection_path);
}

void RunFreestandingKernelPhase136DeviceFailureContainmentProbe(const std::filesystem::path& source_root,
                                                                const std::filesystem::path& binary_root,
                                                                const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase136_device_failure_containment_probe.mirproj.txt";
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
                                                                 build_dir / "kernel_phase136_device_failure_containment_build_output.txt",
                                                                 "freestanding kernel phase136 device failure containment build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase136 freestanding kernel device failure containment build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase136BehaviorSlice(build_dir,
                                build_targets,
                                ResolveFreestandingKernelGoldenPath(source_root,
                                                                    "phase136_device_failure_containment_probe.run.contains.txt"));
    ExpectPhase136MirStructureSlice(dump_targets.mir, mir_projection_path);
}

void RunFreestandingKernelPhase137OptionalDmaOrEquivalentFollowThrough(const std::filesystem::path& source_root,
                                                                       const std::filesystem::path& binary_root,
                                                                       const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase137_optional_dma_or_equivalent_follow_through.mirproj.txt";
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
                                                                 build_dir / "kernel_phase137_optional_dma_follow_through_build_output.txt",
                                                                 "freestanding kernel phase137 optional dma-or-equivalent follow-through build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase137 freestanding kernel optional dma-or-equivalent follow-through build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase137BehaviorSlice(build_dir,
                                build_targets,
                                ResolveFreestandingKernelGoldenPath(source_root,
                                                                    "phase137_optional_dma_or_equivalent_follow_through.run.contains.txt"));
    ExpectPhase137MirStructureSlice(dump_targets.mir, mir_projection_path);
}

void RunFreestandingKernelShard7(const std::filesystem::path& source_root,
                                 const std::filesystem::path& binary_root,
                                 const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const auto artifacts = BuildAndRunFreestandingKernelTarget(mc_path,
                                                               common_paths.project_path,
                                                               common_paths.main_source_path,
                                                               binary_root / "kernel_build",
                                                               "kernel_shard7",
                                                               "freestanding kernel shard7 build",
                                                               "freestanding kernel shard7 run");

    ExpectTextContainsLinesFile(artifacts.run_output,
                                ResolveFreestandingKernelGoldenPath(source_root, "kernel_shard7.run.contains.txt"),
                                "shard7 freestanding kernel run should preserve the landed shard7 slices");

    static constexpr std::string_view kPhase133Selectors[] = {
        "TypeDecl kind=struct name=debug.Phase133MessageLifetimeAudit",
        "Function name=ipc.close_runtime_endpoint returns=[ipc.EndpointCloseTransition]",
        "Function name=ipc.close_runtime_endpoints_for_owner returns=[ipc.EndpointOwnerCloseTransition]",
        "Function name=debug.validate_phase133_message_lifetime_and_reuse returns=[bool]",
    };
    static constexpr std::string_view kPhase134Objects[] = {
        "_Users_ro_dev_c_modern_kernel_src_uart.mc.o",
        "_Users_ro_dev_c_modern_kernel_src_serial_service.mc.o",
    };
    static constexpr std::string_view kPhase134Selectors[] = {
        "TypeDecl kind=struct name=debug.Phase134MinimalDeviceServiceHandoffAudit",
        "Function name=ipc.publish_runtime_byte returns=[ipc.RuntimePublishResult]",
        "Function name=uart.handle_receive_interrupt returns=[uart.UartInterruptResult]",
        "Function name=bootstrap_services.execute_serial_service_receive_and_exit returns=[bootstrap_services.SerialServiceExecutionResult]",
        "Function name=debug.validate_phase134_minimal_device_service_handoff returns=[bool]",
    };
    static constexpr std::string_view kPhase135Objects[] = {
        "_Users_ro_dev_c_modern_kernel_src_uart.mc.o",
        "_Users_ro_dev_c_modern_kernel_src_serial_service.mc.o",
    };
    static constexpr std::string_view kPhase135Selectors[] = {
        "TypeDecl kind=struct name=debug.Phase135BufferOwnershipAudit",
        "Function name=ipc.publish_runtime_frame returns=[ipc.RuntimePublishResult]",
        "Function name=uart.stage_receive_frame returns=[uart.UartDevice]",
        "Function name=bootstrap_services.execute_serial_service_receive returns=[bootstrap_services.SerialServiceExecutionResult]",
        "Function name=debug.validate_phase135_buffer_ownership_boundary returns=[bool]",
    };
    static constexpr std::string_view kPhase136Objects[] = {
        "_Users_ro_dev_c_modern_kernel_src_uart.mc.o",
        "_Users_ro_dev_c_modern_kernel_src_serial_service.mc.o",
    };
    static constexpr std::string_view kPhase136Selectors[] = {
        "TypeDecl kind=struct name=debug.Phase136DeviceFailureContainmentAudit",
        "Function name=uart.handle_receive_interrupt returns=[uart.UartInterruptResult]",
        "Function name=serial_service.record_ingress returns=[serial_service.SerialServiceState]",
        "Function name=bootstrap_services.close_serial_service_after_failure returns=[bootstrap_services.SerialServiceExecutionResult]",
        "Function name=debug.validate_phase136_device_failure_containment returns=[bool]",
    };
    static constexpr std::string_view kPhase137Objects[] = {
        "_Users_ro_dev_c_modern_kernel_src_uart.mc.o",
        "_Users_ro_dev_c_modern_kernel_src_interrupt.mc.o",
        "_Users_ro_dev_c_modern_kernel_src_serial_service.mc.o",
    };
    static constexpr std::string_view kPhase137Selectors[] = {
        "TypeDecl kind=struct name=debug.Phase137OptionalDmaOrEquivalentAudit",
        "Function name=uart.handle_receive_completion_interrupt returns=[uart.UartCompletionInterruptResult]",
        "Function name=interrupt.dispatch_interrupt returns=[interrupt.InterruptDispatchResult]",
        "Function name=serial_service.record_ingress returns=[serial_service.SerialServiceState]",
        "Function name=debug.validate_phase137_optional_dma_or_equivalent_follow_through returns=[bool]",
    };

    const FreestandingKernelPhaseCheck phase_checks[] = {
        {"phase133 message-lifetime audit", "phase133_message_lifetime_and_reuse.run.contains.txt", std::span<const std::string_view>{}, std::span{kPhase133Selectors}, "phase133_message_lifetime_and_reuse.mirproj.txt"},
        {"phase134 device-service handoff audit", "phase134_minimal_device_service_handoff.run.contains.txt", std::span{kPhase134Objects}, std::span{kPhase134Selectors}, "phase134_minimal_device_service_handoff.mirproj.txt"},
        {"phase135 buffer-ownership audit", "phase135_buffer_ownership_boundary_audit.run.contains.txt", std::span{kPhase135Objects}, std::span{kPhase135Selectors}, "phase135_buffer_ownership_boundary_audit.mirproj.txt"},
        {"phase136 failure-containment audit", "phase136_device_failure_containment_probe.run.contains.txt", std::span{kPhase136Objects}, std::span{kPhase136Selectors}, "phase136_device_failure_containment_probe.mirproj.txt"},
        {"phase137 dma-equivalent audit", "phase137_optional_dma_or_equivalent_follow_through.run.contains.txt", std::span{kPhase137Objects}, std::span{kPhase137Selectors}, "phase137_optional_dma_or_equivalent_follow_through.mirproj.txt"},
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
