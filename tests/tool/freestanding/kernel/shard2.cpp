#include <filesystem>
#include <string>
#include <vector>

#include "compiler/support/dump_paths.h"
#include "compiler/support/target.h"
#include "tests/support/process_utils.h"
#include "tests/tool/tool_suite_common.h"

namespace mc::tool_tests {

using mc::test_support::ExpectOutputContains;
using mc::test_support::Fail;
using mc::test_support::ReadFile;
using mc::test_support::RunCommandCapture;

namespace {

void ExpectPhase107BehaviorSlice(std::string_view run_output) {
    ExpectOutputContains(run_output,
                         "PASS phase107_real_user_to_user_capability_transfer",
                         "phase107 freestanding kernel user-to-user capability transfer run should preserve the landed phase107 slice");
}

void ExpectPhase107MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "Function name=validate_phase106_echo_service_request_reply returns=[bool]",
            "TypeDecl kind=struct name=transfer_service.TransferServiceState",
            "TypeDecl kind=struct name=transfer_service.TransferObservation",
            "Function name=execute_phase107_user_to_user_capability_transfer returns=[bool]",
            "Function name=transfer_service.record_grant returns=[transfer_service.TransferServiceState]",
            "Function name=transfer_service.emit_payload returns=[[4]u8]",
            "Function name=transfer_service.observe_transfer returns=[transfer_service.TransferObservation]",
            "Function name=validate_phase107_user_to_user_capability_transfer returns=[bool]",
        },
        expected_projection_path,
        "phase107 merged MIR should preserve the transfer-service projection");
}

void ExpectPhase108BehaviorSlice(std::string_view run_output) {
    ExpectOutputContains(run_output,
                         "PASS phase108_kernel_image_program_cap_audit",
                         "phase108 freestanding kernel image-and-program-cap audit run should preserve the landed phase108 slice");
}

void ExpectPhase108ManifestSlice(const std::filesystem::path& manifest_path,
                                 const std::filesystem::path& expected_lines_path) {
    ExpectTextContainsLinesFile(ReadFile(manifest_path),
                                expected_lines_path,
                                "phase108 kernel manifest should preserve the executable-owned target contract");
}

void ExpectPhase108MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "Function name=validate_phase107_user_to_user_capability_transfer returns=[bool]",
            "VarGlobal names=[INIT_PROGRAM_CAPABILITY] type=capability.CapabilitySlot",
            "VarGlobal names=[LOG_SERVICE_PROGRAM_CAPABILITY] type=capability.CapabilitySlot",
            "VarGlobal names=[ECHO_SERVICE_PROGRAM_CAPABILITY] type=capability.CapabilitySlot",
            "VarGlobal names=[TRANSFER_SERVICE_PROGRAM_CAPABILITY] type=capability.CapabilitySlot",
            "Function name=build_phase108_program_cap_contract returns=[debug.Phase108ProgramCapContract]",
            "Function name=bootstrap_services.seed_log_service_program_capability returns=[bootstrap_services.LogServiceExecutionState]",
            "Function name=bootstrap_services.seed_echo_service_program_capability returns=[bootstrap_services.EchoServiceExecutionState]",
            "Function name=bootstrap_services.seed_transfer_service_program_capability returns=[bootstrap_services.TransferServiceExecutionState]",
            "Function name=debug.validate_phase108_kernel_image_and_program_cap_contracts returns=[bool]",
        },
        expected_projection_path,
        "phase108 merged MIR should preserve the kernel-image and program-cap projection");
}

void ExpectPhase109BehaviorSlice(std::string_view run_output) {
    ExpectOutputContains(run_output,
                         "PASS phase109_first_running_kernel_slice_audit",
                         "phase109 freestanding kernel running-slice audit run should preserve the landed phase109 slice");
}

void ExpectPhase109MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "Function name=debug.validate_phase108_kernel_image_and_program_cap_contracts returns=[bool]",
            "Function name=debug.validate_phase109_first_running_kernel_slice returns=[bool]",
        },
        expected_projection_path,
        "phase109 merged MIR should preserve the first-running-kernel projection");
}

void ExpectPhase110BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets,
                                 std::string_view run_output) {
    ExpectOutputContains(run_output,
                         "PASS phase110_kernel_ownership_split_audit",
                         "phase110 freestanding kernel ownership-split audit run should preserve the landed phase110 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "kernel__debug.o")) {
        Fail("phase110 ownership split should emit a distinct debug module object");
    }
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_sched.mc.o")) {
        Fail("phase110 ownership split should emit a distinct sched module object");
    }
}

void ExpectPhase110MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "Function name=debug.validate_phase108_kernel_image_and_program_cap_contracts returns=[bool]",
            "Function name=debug.validate_phase109_first_running_kernel_slice returns=[bool]",
            "Function name=debug.validate_phase110_kernel_ownership_split returns=[bool]",
            "Function name=sched.validate_program_cap_spawn_and_wait returns=[bool]",
        },
        expected_projection_path,
        "phase110 merged MIR should preserve the ownership split projection");
}


void ExpectPhase111BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets,
                                 std::string_view run_output) {
    ExpectOutputContains(run_output,
                         "PASS phase111_scheduler_lifecycle_ownership_clarification",
                         "phase111 freestanding kernel lifecycle-ownership audit run should preserve the landed phase111 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_lifecycle.mc.o")) {
        Fail("phase111 lifecycle clarification should emit a distinct lifecycle module object");
    }
}

void ExpectPhase111MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "Function name=lifecycle.install_spawned_child returns=[lifecycle.SpawnInstallResult]",
            "Function name=lifecycle.block_task_on_timer returns=[lifecycle.TaskTransition]",
            "Function name=lifecycle.ready_task returns=[lifecycle.TaskTransition]",
            "Function name=lifecycle.validate_task_transition_contracts returns=[bool]",
            "Function name=debug.validate_phase111_scheduler_and_lifecycle_ownership returns=[bool]",
        },
        expected_projection_path,
        "phase111 merged MIR should preserve the lifecycle ownership projection");
}


}  // namespace

void RunFreestandingKernelPhase107RealUserToUserCapabilityTransfer(const std::filesystem::path& source_root,
                                                                   const std::filesystem::path& binary_root,
                                                                   const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path mir_projection_path = ResolveFreestandingKernelGoldenPath(source_root,
                                                                       "phase107_real_user_to_user_capability_transfer.mirproj.txt");
    const auto artifacts = BuildAndRunFreestandingKernelTarget(mc_path,
                                                               common_paths.project_path,
                                                               common_paths.main_source_path,
                                                               binary_root / "kernel_build",
                                                               "kernel_phase107_user_transfer",
                                                               "freestanding kernel phase107 user-to-user capability transfer build",
                                                               "freestanding kernel phase107 user-to-user capability transfer run");
    ExpectPhase107BehaviorSlice(artifacts.run_output);
    ExpectPhase107MirStructureSlice(artifacts.dump_targets.mir, mir_projection_path);
}

void RunFreestandingKernelPhase108KernelImageProgramCapAudit(const std::filesystem::path& source_root,
                                                             const std::filesystem::path& binary_root,
                                                             const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path manifest_lines_path = ResolveFreestandingKernelGoldenPath(source_root,
                                                                       "phase108_kernel_manifest.contains.txt");
    const std::filesystem::path mir_projection_path = ResolveFreestandingKernelGoldenPath(source_root,
                                                                       "phase108_kernel_image_program_cap_audit.mirproj.txt");
    const std::filesystem::path manual_run_lines_path = ResolveFreestandingKernelGoldenPath(source_root,
                                                                         "phase108_manual_kernel_image.run.contains.txt");
    const auto artifacts = BuildAndRunFreestandingKernelTarget(mc_path,
                                                               common_paths.project_path,
                                                               common_paths.main_source_path,
                                                               binary_root / "kernel_build",
                                                               "kernel_phase108_image_program_cap",
                                                               "freestanding kernel phase108 image-and-program-cap audit build",
                                                               "freestanding kernel phase108 image-and-program-cap audit run");
    ExpectPhase108ManifestSlice(common_paths.project_path, manifest_lines_path);

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
    if (!std::filesystem::exists(artifacts.build_targets.object.parent_path() / "_Users_ro_dev_c_modern_kernel_src_address_space.mc.o")) {
        Fail("phase108 kernel image audit should emit separate module objects for imported kernel modules");
    }

    ExpectPhase108BehaviorSlice(artifacts.run_output);

    const std::filesystem::path manual_executable = artifacts.build_dir / "bin" / "phase108_manual_kernel_image";
    std::vector<std::filesystem::path> manual_link_objects;
    for (const auto& entry : std::filesystem::directory_iterator(artifacts.build_targets.object.parent_path())) {
        if (entry.path().extension() == ".o") {
            manual_link_objects.push_back(entry.path());
        }
    }
    LinkBootstrapObjectsAndExpectSuccess(manual_link_objects,
                                         manual_executable,
                                         artifacts.build_dir / "phase108_manual_link_output.txt",
                                         "phase108 manual kernel image relink");

    const auto [manual_run_outcome, manual_run_output] = RunCommandCapture({manual_executable.generic_string()},
                                                                           artifacts.build_dir / "phase108_manual_link_run_output.txt",
                                                                           "phase108 manual kernel image run");
    if (!manual_run_outcome.exited || manual_run_outcome.exit_code != 0) {
        Fail("phase108 manual kernel image run should preserve the landed phase108 slice after target-owned relink\n" + manual_run_output);
    }
    ExpectTextContainsLinesFile(manual_run_output,
                                manual_run_lines_path,
                                "phase108 manual kernel image run should preserve the landed phase108 slice after target-owned relink");
    ExpectPhase108MirStructureSlice(artifacts.dump_targets.mir, mir_projection_path);
}

void RunFreestandingKernelPhase109FirstRunningKernelSliceAudit(const std::filesystem::path& source_root,
                                                               const std::filesystem::path& binary_root,
                                                               const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path mir_projection_path = ResolveFreestandingKernelGoldenPath(source_root,
                                                                       "phase109_first_running_kernel_slice_audit.mirproj.txt");
    const auto artifacts = BuildAndRunFreestandingKernelTarget(mc_path,
                                                               common_paths.project_path,
                                                               common_paths.main_source_path,
                                                               binary_root / "kernel_build",
                                                               "kernel_phase109_running_slice",
                                                               "freestanding kernel phase109 running-slice audit build",
                                                               "freestanding kernel phase109 running-slice audit run");
    ExpectPhase109BehaviorSlice(artifacts.run_output);
    ExpectPhase109MirStructureSlice(artifacts.dump_targets.mir, mir_projection_path);
}

void RunFreestandingKernelPhase110KernelOwnershipSplitAudit(const std::filesystem::path& source_root,
                                                            const std::filesystem::path& binary_root,
                                                            const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase110_kernel_ownership_split_audit.mirproj.txt";
    const auto artifacts = BuildAndRunFreestandingKernelTarget(mc_path,
                                                               common_paths.project_path,
                                                               common_paths.main_source_path,
                                                               binary_root / "kernel_build",
                                                               "kernel_phase110_ownership_split",
                                                               "freestanding kernel phase110 ownership-split audit build",
                                                               "freestanding kernel phase110 ownership-split audit run");
    ExpectPhase110BehaviorSlice(artifacts.build_dir, artifacts.build_targets, artifacts.run_output);
    ExpectPhase110MirStructureSlice(artifacts.dump_targets.mir, mir_projection_path);
}

void RunFreestandingKernelPhase111SchedulerLifecycleOwnershipClarification(const std::filesystem::path& source_root,
                                                                           const std::filesystem::path& binary_root,
                                                                           const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase111_scheduler_lifecycle_ownership_clarification.mirproj.txt";
    const auto artifacts = BuildAndRunFreestandingKernelTarget(mc_path,
                                                               common_paths.project_path,
                                                               common_paths.main_source_path,
                                                               binary_root / "kernel_build",
                                                               "kernel_phase111_lifecycle_ownership",
                                                               "freestanding kernel phase111 lifecycle-ownership audit build",
                                                               "freestanding kernel phase111 lifecycle-ownership audit run");
    ExpectPhase111BehaviorSlice(artifacts.build_dir, artifacts.build_targets, artifacts.run_output);
    ExpectPhase111MirStructureSlice(artifacts.dump_targets.mir, mir_projection_path);
}

void RunFreestandingKernelShard2(const std::filesystem::path& source_root,
                                 const std::filesystem::path& binary_root,
                                 const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path shard_run_lines_path = ResolveFreestandingKernelGoldenPath(source_root,
                                                                        "kernel_shard2.run.contains.txt");
    const std::filesystem::path manifest_lines_path = ResolveFreestandingKernelGoldenPath(source_root,
                                                                       "phase108_kernel_manifest.contains.txt");
    const std::filesystem::path phase107_projection_path = ResolveFreestandingKernelGoldenPath(source_root,
                                                                            "phase107_real_user_to_user_capability_transfer.mirproj.txt");
    const std::filesystem::path phase108_projection_path = ResolveFreestandingKernelGoldenPath(source_root,
                                                                            "phase108_kernel_image_program_cap_audit.mirproj.txt");
    const std::filesystem::path phase109_projection_path = ResolveFreestandingKernelGoldenPath(source_root,
                                                                            "phase109_first_running_kernel_slice_audit.mirproj.txt");
    const std::filesystem::path manual_run_lines_path = ResolveFreestandingKernelGoldenPath(source_root,
                                                                         "phase108_manual_kernel_image.run.contains.txt");
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
    ExpectPhase110BehaviorSlice(artifacts.build_dir, artifacts.build_targets, artifacts.run_output);
    ExpectPhase111BehaviorSlice(artifacts.build_dir, artifacts.build_targets, artifacts.run_output);
    ExpectPhase108ManifestSlice(common_paths.project_path, manifest_lines_path);

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
    if (!std::filesystem::exists(artifacts.build_targets.object.parent_path() / "_Users_ro_dev_c_modern_kernel_src_address_space.mc.o")) {
        Fail("phase108 kernel image audit should emit separate module objects for imported kernel modules");
    }

    const std::filesystem::path manual_executable = artifacts.build_dir / "bin" / "phase108_manual_kernel_image";
    std::vector<std::filesystem::path> manual_link_objects;
    for (const auto& entry : std::filesystem::directory_iterator(artifacts.build_targets.object.parent_path())) {
        if (entry.path().extension() == ".o") {
            manual_link_objects.push_back(entry.path());
        }
    }
    LinkBootstrapObjectsAndExpectSuccess(manual_link_objects,
                                         manual_executable,
                                         artifacts.build_dir / "kernel_shard2_phase108_manual_link_output.txt",
                                         "phase108 manual kernel image relink");

    const auto [manual_run_outcome, manual_run_output] = RunCommandCapture({manual_executable.generic_string()},
                                                                           artifacts.build_dir / "kernel_shard2_phase108_manual_link_run_output.txt",
                                                                           "phase108 manual kernel image run");
    if (!manual_run_outcome.exited || manual_run_outcome.exit_code != 0) {
        Fail("phase108 manual kernel image run should preserve the landed phase108 slice after target-owned relink\n" + manual_run_output);
    }
    ExpectTextContainsLinesFile(manual_run_output,
                                manual_run_lines_path,
                                "phase108 manual kernel image run should preserve the landed phase108 slice after target-owned relink");

    ExpectPhase107MirStructureSlice(artifacts.dump_targets.mir, phase107_projection_path);
    ExpectPhase108MirStructureSlice(artifacts.dump_targets.mir, phase108_projection_path);
    ExpectPhase109MirStructureSlice(artifacts.dump_targets.mir, phase109_projection_path);

    const std::filesystem::path phase110_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                           "phase110_kernel_ownership_split_audit.mirproj.txt";
    const std::filesystem::path phase111_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                           "phase111_scheduler_lifecycle_ownership_clarification.mirproj.txt";
    ExpectPhase110MirStructureSlice(artifacts.dump_targets.mir, phase110_projection_path);
    ExpectPhase111MirStructureSlice(artifacts.dump_targets.mir, phase111_projection_path);
}

}  // namespace mc::tool_tests
