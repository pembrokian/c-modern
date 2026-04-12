#include <array>
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

constexpr std::array<std::string_view, 0> kNoRequiredObjectFiles{};

constexpr std::array<std::string_view, 8> kPhase107MirSelectors{ {
    "Function name=validate_phase106_echo_service_request_reply returns=[bool]",
    "TypeDecl kind=struct name=transfer_service.TransferServiceState",
    "TypeDecl kind=struct name=transfer_service.TransferObservation",
    "Function name=execute_phase107_user_to_user_capability_transfer returns=[bool]",
    "Function name=transfer_service.record_grant returns=[transfer_service.TransferServiceState]",
    "Function name=transfer_service.emit_payload returns=[[4]u8]",
    "Function name=transfer_service.observe_transfer returns=[transfer_service.TransferObservation]",
    "Function name=validate_phase107_user_to_user_capability_transfer returns=[bool]",
} };

constexpr std::array<std::string_view, 1> kPhase108RequiredObjectFiles{ {
    "_Users_ro_dev_c_modern_kernel_src_address_space.mc.o",
} };

constexpr std::array<std::string_view, 10> kPhase108MirSelectors{ {
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
} };

constexpr std::array<std::string_view, 2> kPhase109MirSelectors{ {
    "Function name=debug.validate_phase108_kernel_image_and_program_cap_contracts returns=[bool]",
    "Function name=debug.validate_phase109_first_running_kernel_slice returns=[bool]",
} };

constexpr std::array<std::string_view, 2> kPhase110RequiredObjectFiles{ {
    "kernel__debug.o",
    "_Users_ro_dev_c_modern_kernel_src_sched.mc.o",
} };

constexpr std::array<std::string_view, 4> kPhase110MirSelectors{ {
    "Function name=debug.validate_phase108_kernel_image_and_program_cap_contracts returns=[bool]",
    "Function name=debug.validate_phase109_first_running_kernel_slice returns=[bool]",
    "Function name=debug.validate_phase110_kernel_ownership_split returns=[bool]",
    "Function name=sched.validate_program_cap_spawn_and_wait returns=[bool]",
} };

constexpr std::array<std::string_view, 1> kPhase111RequiredObjectFiles{ {
    "_Users_ro_dev_c_modern_kernel_src_lifecycle.mc.o",
} };

constexpr std::array<std::string_view, 5> kPhase111MirSelectors{ {
    "Function name=lifecycle.install_spawned_child returns=[lifecycle.SpawnInstallResult]",
    "Function name=lifecycle.block_task_on_timer returns=[lifecycle.TaskTransition]",
    "Function name=lifecycle.ready_task returns=[lifecycle.TaskTransition]",
    "Function name=lifecycle.validate_task_transition_contracts returns=[bool]",
    "Function name=debug.validate_phase111_scheduler_and_lifecycle_ownership returns=[bool]",
} };

const FreestandingKernelPhaseCheck kPhase107Check{
    .label = "phase107 freestanding kernel user-to-user capability transfer",
    .expected_run_lines_file = "phase107_real_user_to_user_capability_transfer.run.contains.txt",
    .required_object_files = kNoRequiredObjectFiles,
    .mir_selectors = kPhase107MirSelectors,
    .expected_mir_projection_file = "phase107_real_user_to_user_capability_transfer.mirproj.txt",
};

const FreestandingKernelPhaseCheck kPhase108Check{
    .label = "phase108 freestanding kernel image-and-program-cap audit",
    .expected_run_lines_file = "phase108_kernel_image_program_cap_audit.run.contains.txt",
    .required_object_files = kPhase108RequiredObjectFiles,
    .mir_selectors = kPhase108MirSelectors,
    .expected_mir_projection_file = "phase108_kernel_image_program_cap_audit.mirproj.txt",
};

const FreestandingKernelPhaseCheck kPhase109Check{
    .label = "phase109 freestanding kernel running-slice audit",
    .expected_run_lines_file = "phase109_first_running_kernel_slice_audit.run.contains.txt",
    .required_object_files = kNoRequiredObjectFiles,
    .mir_selectors = kPhase109MirSelectors,
    .expected_mir_projection_file = "phase109_first_running_kernel_slice_audit.mirproj.txt",
};

const FreestandingKernelPhaseCheck kPhase110Check{
    .label = "phase110 freestanding kernel ownership-split audit",
    .expected_run_lines_file = "phase110_kernel_ownership_split_audit.run.contains.txt",
    .required_object_files = kPhase110RequiredObjectFiles,
    .mir_selectors = kPhase110MirSelectors,
    .expected_mir_projection_file = "phase110_kernel_ownership_split_audit.mirproj.txt",
};

const FreestandingKernelPhaseCheck kPhase111Check{
    .label = "phase111 freestanding kernel lifecycle-ownership audit",
    .expected_run_lines_file = "phase111_scheduler_lifecycle_ownership_clarification.run.contains.txt",
    .required_object_files = kPhase111RequiredObjectFiles,
    .mir_selectors = kPhase111MirSelectors,
    .expected_mir_projection_file = "phase111_scheduler_lifecycle_ownership_clarification.mirproj.txt",
};

const std::array<FreestandingKernelPhaseCheck, 5> kShard2PhaseChecks{ {
    kPhase107Check,
    kPhase108Check,
    kPhase109Check,
    kPhase110Check,
    kPhase111Check,
} };

void ExpectPhase108ManifestSlice(const std::filesystem::path& manifest_path,
                                 const std::filesystem::path& expected_lines_path) {
    ExpectTextContainsLinesFile(ReadFile(manifest_path),
                                expected_lines_path,
                                "phase108 kernel manifest should preserve the executable-owned target contract");
}

}  // namespace

void RunFreestandingKernelPhase107RealUserToUserCapabilityTransfer(const std::filesystem::path& source_root,
                                                                   const std::filesystem::path& binary_root,
                                                                   const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const auto artifacts = BuildAndRunFreestandingKernelTarget(mc_path,
                                                               common_paths.project_path,
                                                               common_paths.main_source_path,
                                                               binary_root / "kernel_build",
                                                               "kernel_phase107_user_transfer",
                                                               "freestanding kernel phase107 user-to-user capability transfer build",
                                                               "freestanding kernel phase107 user-to-user capability transfer run");
    const std::string kernel_mir = ReadFile(artifacts.dump_targets.mir);
    ExpectFreestandingKernelPhaseFromArtifacts(source_root,
                                               artifacts.build_targets.object.parent_path(),
                                               artifacts.run_output,
                                               kernel_mir,
                                               kPhase107Check);
}

void RunFreestandingKernelPhase108KernelImageProgramCapAudit(const std::filesystem::path& source_root,
                                                             const std::filesystem::path& binary_root,
                                                             const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path manifest_lines_path = ResolveFreestandingKernelGoldenPath(source_root,
                                                                       "phase108_kernel_manifest.contains.txt");
    const auto artifacts = BuildAndRunFreestandingKernelTarget(mc_path,
                                                               common_paths.project_path,
                                                               common_paths.main_source_path,
                                                               binary_root / "kernel_build",
                                                               "kernel_phase108_image_program_cap",
                                                               "freestanding kernel phase108 image-and-program-cap audit build",
                                                               "freestanding kernel phase108 image-and-program-cap audit run");
    ExpectPhase108ManifestSlice(common_paths.project_path, manifest_lines_path);
    const std::string kernel_mir = ReadFile(artifacts.dump_targets.mir);
    ExpectFreestandingKernelPhaseFromArtifacts(source_root,
                                               artifacts.build_targets.object.parent_path(),
                                               artifacts.run_output,
                                               kernel_mir,
                                               kPhase108Check);
}

void RunFreestandingKernelPhase109FirstRunningKernelSliceAudit(const std::filesystem::path& source_root,
                                                               const std::filesystem::path& binary_root,
                                                               const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const auto artifacts = BuildAndRunFreestandingKernelTarget(mc_path,
                                                               common_paths.project_path,
                                                               common_paths.main_source_path,
                                                               binary_root / "kernel_build",
                                                               "kernel_phase109_running_slice",
                                                               "freestanding kernel phase109 running-slice audit build",
                                                               "freestanding kernel phase109 running-slice audit run");
    const std::string kernel_mir = ReadFile(artifacts.dump_targets.mir);
    ExpectFreestandingKernelPhaseFromArtifacts(source_root,
                                               artifacts.build_targets.object.parent_path(),
                                               artifacts.run_output,
                                               kernel_mir,
                                               kPhase109Check);
}

void RunFreestandingKernelPhase110KernelOwnershipSplitAudit(const std::filesystem::path& source_root,
                                                            const std::filesystem::path& binary_root,
                                                            const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const auto artifacts = BuildAndRunFreestandingKernelTarget(mc_path,
                                                               common_paths.project_path,
                                                               common_paths.main_source_path,
                                                               binary_root / "kernel_build",
                                                               "kernel_phase110_ownership_split",
                                                               "freestanding kernel phase110 ownership-split audit build",
                                                               "freestanding kernel phase110 ownership-split audit run");
    const std::string kernel_mir = ReadFile(artifacts.dump_targets.mir);
    ExpectFreestandingKernelPhaseFromArtifacts(source_root,
                                               artifacts.build_targets.object.parent_path(),
                                               artifacts.run_output,
                                               kernel_mir,
                                               kPhase110Check);
}

void RunFreestandingKernelPhase111SchedulerLifecycleOwnershipClarification(const std::filesystem::path& source_root,
                                                                           const std::filesystem::path& binary_root,
                                                                           const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const auto artifacts = BuildAndRunFreestandingKernelTarget(mc_path,
                                                               common_paths.project_path,
                                                               common_paths.main_source_path,
                                                               binary_root / "kernel_build",
                                                               "kernel_phase111_lifecycle_ownership",
                                                               "freestanding kernel phase111 lifecycle-ownership audit build",
                                                               "freestanding kernel phase111 lifecycle-ownership audit run");
    const std::string kernel_mir = ReadFile(artifacts.dump_targets.mir);
    ExpectFreestandingKernelPhaseFromArtifacts(source_root,
                                               artifacts.build_targets.object.parent_path(),
                                               artifacts.run_output,
                                               kernel_mir,
                                               kPhase111Check);
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
    ExpectPhase108ManifestSlice(common_paths.project_path, manifest_lines_path);
    const std::string kernel_mir = ReadFile(artifacts.dump_targets.mir);
    for (const auto& phase_check : kShard2PhaseChecks) {
        ExpectFreestandingKernelPhaseFromArtifacts(source_root,
                                                   artifacts.build_targets.object.parent_path(),
                                                   artifacts.run_output,
                                                   kernel_mir,
                                                   phase_check);
    }
}

}  // namespace mc::tool_tests
