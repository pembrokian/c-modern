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

void ExpectPhase117BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets,
                                 const std::filesystem::path& expected_lines_path) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase117_multi_service_run_output.txt",
                                                             "freestanding kernel phase117 multi-service run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("phase117 freestanding kernel multi-service run should preserve the landed phase117 slice\n" + run_output);
    }
    ExpectTextContainsLinesFile(run_output,
                                expected_lines_path,
                                "phase117 freestanding kernel multi-service run should preserve the landed phase117 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "kernel__bootstrap_services.o")) {
        Fail("phase117 multi-service audit should emit the bootstrap_services module object");
    }
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_log_service.mc.o")) {
        Fail("phase117 multi-service audit should emit the log_service module object");
    }
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_echo_service.mc.o")) {
        Fail("phase117 multi-service audit should emit the echo_service module object");
    }
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_transfer_service.mc.o")) {
        Fail("phase117 multi-service audit should emit the transfer_service module object");
    }
}

void ExpectPhase117MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "TypeDecl kind=struct name=debug.Phase117MultiServiceBringUpAudit",
            "Function name=bootstrap_audit.build_phase117_multi_service_bring_up_audit returns=[debug.Phase117MultiServiceBringUpAudit]",
            "Function name=debug.validate_phase117_init_orchestrated_multi_service_bring_up returns=[bool]",
        },
        expected_projection_path,
        "phase117 merged MIR should preserve the aggregate multi-service projection");
}


void ExpectPhase118BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets,
                                 const std::filesystem::path& expected_lines_path) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase118_delegation_run_output.txt",
                                                             "freestanding kernel phase118 request-reply and delegation run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("phase118 freestanding kernel delegated request-reply run should preserve the landed phase118 slice\n" + run_output);
    }
    ExpectTextContainsLinesFile(run_output,
                                expected_lines_path,
                                "phase118 freestanding kernel delegated request-reply run should preserve the landed phase118 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_transfer_service.mc.o")) {
        Fail("phase118 delegated request-reply audit should emit the transfer_service module object");
    }
}

void ExpectPhase118MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "TypeDecl kind=struct name=debug.Phase118DelegatedRequestReplyAudit",
            "Function name=bootstrap_audit.build_phase118_delegated_request_reply_audit returns=[debug.Phase118DelegatedRequestReplyAudit]",
            "Function name=debug.validate_phase118_request_reply_and_delegation_follow_through returns=[bool]",
        },
        expected_projection_path,
        "phase118 merged MIR should preserve the delegated request-reply projection");
}


void ExpectPhase119BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets,
                                 const std::filesystem::path& expected_lines_path) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase119_namespace_run_output.txt",
                                                             "freestanding kernel phase119 namespace-pressure run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("phase119 freestanding kernel namespace-pressure run should preserve the landed phase119 slice\n" + run_output);
    }
    ExpectTextContainsLinesFile(run_output,
                                expected_lines_path,
                                "phase119 freestanding kernel namespace-pressure run should preserve the landed phase119 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "kernel__bootstrap_audit.o")) {
        Fail("phase119 namespace-pressure audit should emit the bootstrap_audit module object");
    }
}

void ExpectPhase119MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "TypeDecl kind=struct name=debug.Phase119NamespacePressureAudit",
            "Function name=bootstrap_audit.build_phase119_namespace_pressure_audit returns=[debug.Phase119NamespacePressureAudit]",
            "Function name=debug.validate_phase119_namespace_pressure_audit returns=[bool]",
        },
        expected_projection_path,
        "phase119 merged MIR should preserve the namespace-pressure projection");
}


void ExpectPhase120BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets,
                                 const std::filesystem::path& expected_lines_path) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase120_support_run_output.txt",
                                                             "freestanding kernel phase120 running-system support run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("phase120 freestanding kernel running-system support run should preserve the landed phase120 slice\n" + run_output);
    }
    ExpectTextContainsLinesFile(run_output,
                                expected_lines_path,
                                "phase120 freestanding kernel running-system support run should preserve the landed phase120 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "kernel__bootstrap_audit.o")) {
        Fail("phase120 running-system support audit should emit the bootstrap_audit module object");
    }
}

void ExpectPhase120MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "TypeDecl kind=struct name=debug.Phase120RunningSystemSupportAudit",
            "Function name=bootstrap_audit.build_phase120_running_system_support_audit returns=[debug.Phase120RunningSystemSupportAudit]",
            "Function name=debug.validate_phase120_running_system_support_statement returns=[bool]",
        },
        expected_projection_path,
        "phase120 merged MIR should preserve the running-system support projection");
}


void ExpectPhase121BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets,
                                 const std::filesystem::path& expected_lines_path) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase121_image_contract_run_output.txt",
                                                             "freestanding kernel phase121 image-contract hardening run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("phase121 freestanding kernel image-contract hardening run should preserve the landed phase121 slice\n" + run_output);
    }
    ExpectTextContainsLinesFile(run_output,
                                expected_lines_path,
                                "phase121 freestanding kernel image-contract hardening run should preserve the landed phase121 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "kernel__bootstrap_audit.o")) {
        Fail("phase121 image-contract hardening audit should emit the bootstrap_audit module object");
    }
}

void ExpectPhase121MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "TypeDecl kind=struct name=debug.Phase121KernelImageContractAudit",
            "Function name=bootstrap_audit.build_phase121_kernel_image_contract_audit returns=[debug.Phase121KernelImageContractAudit]",
            "Function name=debug.validate_phase121_kernel_image_contract_hardening returns=[bool]",
        },
        expected_projection_path,
        "phase121 merged MIR should preserve the image-contract projection");
}


}  // namespace

void RunFreestandingKernelPhase117InitOrchestratedMultiServiceBringUp(const std::filesystem::path& source_root,
                                                                      const std::filesystem::path& binary_root,
                                                                      const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase117_init_orchestrated_multi_service_bring_up.mirproj.txt";
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
                                                                 build_dir / "kernel_phase117_multi_service_build_output.txt",
                                                                 "freestanding kernel phase117 multi-service build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase117 freestanding kernel multi-service build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase117BehaviorSlice(build_dir,
                                build_targets,
                                ResolveFreestandingKernelGoldenPath(source_root,
                                                                    "phase117_init_orchestrated_multi_service_bring_up.run.contains.txt"));
    ExpectPhase117MirStructureSlice(dump_targets.mir, mir_projection_path);
}

void RunFreestandingKernelPhase118RequestReplyAndDelegationFollowThrough(const std::filesystem::path& source_root,
                                                                         const std::filesystem::path& binary_root,
                                                                         const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase118_request_reply_and_delegation_follow_through.mirproj.txt";
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
                                                                 build_dir / "kernel_phase118_delegation_build_output.txt",
                                                                 "freestanding kernel phase118 request-reply and delegation build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase118 freestanding kernel delegated request-reply build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase118BehaviorSlice(build_dir,
                                build_targets,
                                ResolveFreestandingKernelGoldenPath(source_root,
                                                                    "phase118_request_reply_and_delegation_follow_through.run.contains.txt"));
    ExpectPhase118MirStructureSlice(dump_targets.mir, mir_projection_path);
}

void RunFreestandingKernelPhase119NamespacePressureAudit(const std::filesystem::path& source_root,
                                                         const std::filesystem::path& binary_root,
                                                         const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase119_namespace_pressure_audit.mirproj.txt";
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
                                                                 build_dir / "kernel_phase119_namespace_build_output.txt",
                                                                 "freestanding kernel phase119 namespace-pressure build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase119 freestanding kernel namespace-pressure build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase119BehaviorSlice(build_dir,
                                build_targets,
                                ResolveFreestandingKernelGoldenPath(source_root,
                                                                    "phase119_namespace_pressure_audit.run.contains.txt"));
    ExpectPhase119MirStructureSlice(dump_targets.mir, mir_projection_path);
}

void RunFreestandingKernelPhase120RunningSystemSupportStatement(const std::filesystem::path& source_root,
                                                                const std::filesystem::path& binary_root,
                                                                const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path support_note_path = source_root / "docs" / "plan" /
                                                    "canopus_running_system_support_statement.txt";
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase120_running_system_support_statement.mirproj.txt";
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
                                                                 build_dir / "kernel_phase120_support_build_output.txt",
                                                                 "freestanding kernel phase120 running-system support build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase120 freestanding kernel running-system support build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase120BehaviorSlice(build_dir,
                                build_targets,
                                ResolveFreestandingKernelGoldenPath(source_root,
                                                                    "phase120_running_system_support_statement.run.contains.txt"));
    ExpectPhase120MirStructureSlice(dump_targets.mir, mir_projection_path);
}

void RunFreestandingKernelPhase121KernelImageContractHardening(const std::filesystem::path& source_root,
                                                               const std::filesystem::path& binary_root,
                                                               const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path support_note_path = source_root / "docs" / "plan" /
                                                    "canopus_running_system_support_statement.txt";
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase121_kernel_image_contract_hardening.mirproj.txt";
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
                                                                 build_dir / "kernel_phase121_image_contract_build_output.txt",
                                                                 "freestanding kernel phase121 image-contract hardening build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase121 freestanding kernel image-contract hardening build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase121BehaviorSlice(build_dir,
                                build_targets,
                                ResolveFreestandingKernelGoldenPath(source_root,
                                                                    "phase121_kernel_image_contract_hardening.run.contains.txt"));
    ExpectPhase121MirStructureSlice(dump_targets.mir, mir_projection_path);
}

void RunFreestandingKernelShard4(const std::filesystem::path& source_root,
                                 const std::filesystem::path& binary_root,
                                 const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const auto artifacts = BuildAndRunFreestandingKernelTarget(mc_path,
                                                               common_paths.project_path,
                                                               common_paths.main_source_path,
                                                               binary_root / "kernel_build",
                                                               "kernel_shard4",
                                                               "freestanding kernel shard4 build",
                                                               "freestanding kernel shard4 run");

    ExpectTextContainsLinesFile(artifacts.run_output,
                                ResolveFreestandingKernelGoldenPath(source_root, "kernel_shard4.run.contains.txt"),
                                "shard4 freestanding kernel run should preserve the landed shard4 slices");
    static constexpr std::string_view kPhase117Objects[] = {
        "kernel__bootstrap_services.o",
        "_Users_ro_dev_c_modern_kernel_src_log_service.mc.o",
        "_Users_ro_dev_c_modern_kernel_src_echo_service.mc.o",
        "_Users_ro_dev_c_modern_kernel_src_transfer_service.mc.o",
    };
    static constexpr std::string_view kPhase117Selectors[] = {
        "TypeDecl kind=struct name=debug.Phase117MultiServiceBringUpAudit",
        "Function name=bootstrap_audit.build_phase117_multi_service_bring_up_audit returns=[debug.Phase117MultiServiceBringUpAudit]",
        "Function name=debug.validate_phase117_init_orchestrated_multi_service_bring_up returns=[bool]",
    };
    static constexpr std::string_view kPhase118Objects[] = {
        "_Users_ro_dev_c_modern_kernel_src_transfer_service.mc.o",
    };
    static constexpr std::string_view kPhase118Selectors[] = {
        "TypeDecl kind=struct name=debug.Phase118DelegatedRequestReplyAudit",
        "Function name=bootstrap_audit.build_phase118_delegated_request_reply_audit returns=[debug.Phase118DelegatedRequestReplyAudit]",
        "Function name=debug.validate_phase118_request_reply_and_delegation_follow_through returns=[bool]",
    };
    static constexpr std::string_view kPhase119Objects[] = {
        "kernel__bootstrap_audit.o",
    };
    static constexpr std::string_view kPhase119Selectors[] = {
        "TypeDecl kind=struct name=debug.Phase119NamespacePressureAudit",
        "Function name=bootstrap_audit.build_phase119_namespace_pressure_audit returns=[debug.Phase119NamespacePressureAudit]",
        "Function name=debug.validate_phase119_namespace_pressure_audit returns=[bool]",
    };
    static constexpr std::string_view kPhase120Objects[] = {
        "kernel__bootstrap_audit.o",
    };
    static constexpr std::string_view kPhase120Selectors[] = {
        "TypeDecl kind=struct name=debug.Phase120RunningSystemSupportAudit",
        "Function name=bootstrap_audit.build_phase120_running_system_support_audit returns=[debug.Phase120RunningSystemSupportAudit]",
        "Function name=debug.validate_phase120_running_system_support_statement returns=[bool]",
    };
    static constexpr std::string_view kPhase121Objects[] = {
        "kernel__bootstrap_audit.o",
    };
    static constexpr std::string_view kPhase121Selectors[] = {
        "TypeDecl kind=struct name=debug.Phase121KernelImageContractAudit",
        "Function name=bootstrap_audit.build_phase121_kernel_image_contract_audit returns=[debug.Phase121KernelImageContractAudit]",
        "Function name=debug.validate_phase121_kernel_image_contract_hardening returns=[bool]",
    };

    const FreestandingKernelPhaseCheck phase_checks[] = {
        {"phase117 multi-service audit", "phase117_init_orchestrated_multi_service_bring_up.run.contains.txt", std::span{kPhase117Objects}, std::span{kPhase117Selectors}, "phase117_init_orchestrated_multi_service_bring_up.mirproj.txt"},
        {"phase118 delegated request-reply audit", "phase118_request_reply_and_delegation_follow_through.run.contains.txt", std::span{kPhase118Objects}, std::span{kPhase118Selectors}, "phase118_request_reply_and_delegation_follow_through.mirproj.txt"},
        {"phase119 namespace-pressure audit", "phase119_namespace_pressure_audit.run.contains.txt", std::span{kPhase119Objects}, std::span{kPhase119Selectors}, "phase119_namespace_pressure_audit.mirproj.txt"},
        {"phase120 running-system support audit", "phase120_running_system_support_statement.run.contains.txt", std::span{kPhase120Objects}, std::span{kPhase120Selectors}, "phase120_running_system_support_statement.mirproj.txt"},
        {"phase121 image-contract audit", "phase121_kernel_image_contract_hardening.run.contains.txt", std::span{kPhase121Objects}, std::span{kPhase121Selectors}, "phase121_kernel_image_contract_hardening.mirproj.txt"},
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
