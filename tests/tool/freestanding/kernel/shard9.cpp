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

void ExpectPhase143BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets,
                                 const std::filesystem::path& expected_lines_path) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase143_long_lived_log_service_run_output.txt",
                                                             "freestanding kernel phase143 long-lived log service run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("phase143 freestanding kernel long-lived log service run should preserve the landed phase143 slice\n" + run_output);
    }
    ExpectTextContainsLinesFile(run_output,
                                expected_lines_path,
                                "phase143 freestanding kernel long-lived log service run should preserve the landed phase143 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_log_service.mc.o")) {
        Fail("phase143 long-lived log service should emit the log_service module object");
    }
}

void ExpectPhase143MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "TypeDecl kind=struct name=debug.Phase143LongLivedLogServiceAudit",
            "Function name=bootstrap_audit.build_phase143_long_lived_log_service_audit returns=[debug.Phase143LongLivedLogServiceAudit]",
            "Function name=log_service.observe_retention returns=[log_service.LogRetentionObservation]",
            "Function name=debug.validate_phase143_long_lived_log_service returns=[bool]",
        },
        expected_projection_path,
        "phase143 merged MIR should preserve the long-lived log-service projection");
}


void ExpectPhase144BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets,
                                 const std::filesystem::path& expected_lines_path) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase144_stateful_key_value_service_run_output.txt",
                                                             "freestanding kernel phase144 stateful key-value service run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("phase144 freestanding kernel stateful key-value service run should preserve the landed phase144 slice\n" + run_output);
    }
    ExpectTextContainsLinesFile(run_output,
                                expected_lines_path,
                                "phase144 freestanding kernel stateful key-value service run should preserve the landed phase144 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_kv_service.mc.o")) {
        Fail("phase144 stateful key-value service should emit the kv_service module object");
    }
    if (!std::filesystem::exists(object_dir / "kernel__bootstrap_services.o")) {
        Fail("phase144 stateful key-value service should emit the bootstrap_services module object");
    }
}

void ExpectPhase144MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "TypeDecl kind=struct name=debug.Phase144StatefulKeyValueServiceFollowThroughAudit",
            "Function name=bootstrap_audit.build_phase144_stateful_key_value_service_follow_through_audit returns=[debug.Phase144StatefulKeyValueServiceFollowThroughAudit]",
            "Function name=bootstrap_services.execute_phase144_stateful_key_value_shell_command returns=[bootstrap_services.Phase142ShellCommandRouteResult]",
            "Function name=debug.validate_phase144_stateful_key_value_service_follow_through returns=[bool]",
        },
        expected_projection_path,
        "phase144 merged MIR should preserve the stateful key-value-service projection");
}


void ExpectPhase145BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets,
                                 const std::filesystem::path& expected_lines_path) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase145_service_restart_failure_and_usage_pressure_run_output.txt",
                                                             "freestanding kernel phase145 service restart failure and usage pressure run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("phase145 freestanding kernel service restart failure and usage pressure run should preserve the landed phase145 slice\n" + run_output);
    }
    ExpectTextContainsLinesFile(run_output,
                                expected_lines_path,
                                "phase145 freestanding kernel service restart failure and usage pressure run should preserve the landed phase145 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_kv_service.mc.o")) {
        Fail("phase145 service restart failure and usage pressure audit should emit the kv_service module object");
    }
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_init.mc.o")) {
        Fail("phase145 service restart failure and usage pressure audit should emit the init module object");
    }
    if (!std::filesystem::exists(object_dir / "kernel__bootstrap_services.o")) {
        Fail("phase145 service restart failure and usage pressure audit should emit the bootstrap_services module object");
    }
}

void ExpectPhase145MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "TypeDecl kind=struct name=debug.Phase145ServiceRestartFailureAndUsagePressureAudit",
            "Function name=bootstrap_audit.build_phase145_service_restart_failure_and_usage_pressure_audit returns=[debug.Phase145ServiceRestartFailureAndUsagePressureAudit]",
            "Function name=bootstrap_services.execute_phase145_service_restart_shell_command returns=[bootstrap_services.Phase142ShellCommandRouteResult]",
            "Function name=debug.validate_phase145_service_restart_failure_and_usage_pressure returns=[bool]",
        },
        expected_projection_path,
        "phase145 merged MIR should preserve the service-restart failure-and-usage projection");
}


void ExpectPhase146BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets,
                                 const std::filesystem::path& expected_lines_path) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase146_service_shape_consolidation_run_output.txt",
                                                             "freestanding kernel phase146 service shape consolidation run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("phase146 freestanding kernel service shape consolidation run should preserve the landed phase146 slice\n" + run_output);
    }
    ExpectTextContainsLinesFile(run_output,
                                expected_lines_path,
                                "phase146 freestanding kernel service shape consolidation run should preserve the landed phase146 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_shell_service.mc.o")) {
        Fail("phase146 service shape consolidation should emit the shell_service module object");
    }
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_log_service.mc.o")) {
        Fail("phase146 service shape consolidation should emit the log_service module object");
    }
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_kv_service.mc.o")) {
        Fail("phase146 service shape consolidation should emit the kv_service module object");
    }
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_serial_service.mc.o")) {
        Fail("phase146 service shape consolidation should emit the serial_service module object");
    }
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_init.mc.o")) {
        Fail("phase146 service shape consolidation should emit the init module object");
    }
}

void ExpectPhase146MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "TypeDecl kind=struct name=debug.Phase146ServiceShapeConsolidationAudit",
            "Function name=bootstrap_audit.build_phase146_service_shape_consolidation_audit returns=[debug.Phase146ServiceShapeConsolidationAudit]",
            "Function name=serial_service.observe_composition returns=[serial_service.SerialCompositionObservation]",
            "Function name=debug.validate_phase146_service_shape_consolidation returns=[bool]",
        },
        expected_projection_path,
        "phase146 merged MIR should preserve the service-shape consolidation projection");
}


void ExpectPhase147BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets,
                                 const std::filesystem::path& expected_lines_path) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase147_ipc_shape_audit_under_real_usage_run_output.txt",
                                                             "freestanding kernel phase147 ipc shape audit under real usage run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("phase147 freestanding kernel ipc shape audit under real usage run should preserve the landed phase147 slice\n" + run_output);
    }
    ExpectTextContainsLinesFile(run_output,
                                expected_lines_path,
                                "phase147 freestanding kernel ipc shape audit under real usage run should preserve the landed phase147 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_shell_service.mc.o")) {
        Fail("phase147 ipc shape audit under real usage should emit the shell_service module object");
    }
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_log_service.mc.o")) {
        Fail("phase147 ipc shape audit under real usage should emit the log_service module object");
    }
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_kv_service.mc.o")) {
        Fail("phase147 ipc shape audit under real usage should emit the kv_service module object");
    }
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_serial_service.mc.o")) {
        Fail("phase147 ipc shape audit under real usage should emit the serial_service module object");
    }
    if (!std::filesystem::exists(object_dir / "kernel__bootstrap_services.o")) {
        Fail("phase147 ipc shape audit under real usage should emit the bootstrap_services module object");
    }
}

void ExpectPhase147MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "TypeDecl kind=struct name=debug.Phase147IpcShapeAudit",
            "Function name=bootstrap_audit.build_phase147_ipc_shape_audit returns=[debug.Phase147IpcShapeAudit]",
            "Function name=bootstrap_services.phase147_ipc_shape_workflow_result returns=[bootstrap_services.Phase147IpcShapeWorkflowResult]",
            "Function name=debug.validate_phase147_ipc_shape_audit_under_real_usage returns=[bool]",
        },
        expected_projection_path,
        "phase147 merged MIR should preserve the repeated-usage IPC-shape projection");
}


}  // namespace

void RunFreestandingKernelPhase143LongLivedLogServiceFollowThrough(const std::filesystem::path& source_root,
                                                                   const std::filesystem::path& binary_root,
                                                                   const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase143_long_lived_log_service_follow_through.mirproj.txt";
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
                                                                 build_dir / "kernel_phase143_long_lived_log_service_build_output.txt",
                                                                 "freestanding kernel phase143 long-lived log service build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase143 freestanding kernel long-lived log service build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase143BehaviorSlice(build_dir,
                                build_targets,
                                ResolveFreestandingKernelGoldenPath(source_root,
                                                                    "phase143_long_lived_log_service_follow_through.run.contains.txt"));
    ExpectPhase143MirStructureSlice(dump_targets.mir, mir_projection_path);
}

void RunFreestandingKernelPhase144StatefulKeyValueServiceFollowThrough(const std::filesystem::path& source_root,
                                                                       const std::filesystem::path& binary_root,
                                                                       const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase144_stateful_key_value_service_follow_through.mirproj.txt";
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
                                                                 build_dir / "kernel_phase144_stateful_key_value_service_build_output.txt",
                                                                 "freestanding kernel phase144 stateful key-value service build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase144 freestanding kernel stateful key-value service build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase144BehaviorSlice(build_dir,
                                build_targets,
                                ResolveFreestandingKernelGoldenPath(source_root,
                                                                    "phase144_stateful_key_value_service_follow_through.run.contains.txt"));
    ExpectPhase144MirStructureSlice(dump_targets.mir, mir_projection_path);
}

void RunFreestandingKernelPhase145ServiceRestartFailureAndUsagePressureAudit(const std::filesystem::path& source_root,
                                                                             const std::filesystem::path& binary_root,
                                                                             const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase145_service_restart_failure_and_usage_pressure_audit.mirproj.txt";
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
                                                                 build_dir / "kernel_phase145_service_restart_failure_and_usage_pressure_build_output.txt",
                                                                 "freestanding kernel phase145 service restart failure and usage pressure build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase145 freestanding kernel service restart failure and usage pressure build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase145BehaviorSlice(build_dir,
                                build_targets,
                                ResolveFreestandingKernelGoldenPath(source_root,
                                                                    "phase145_service_restart_failure_and_usage_pressure_audit.run.contains.txt"));
    ExpectPhase145MirStructureSlice(dump_targets.mir, mir_projection_path);
}

void RunFreestandingKernelPhase146ServiceShapeConsolidation(const std::filesystem::path& source_root,
                                                            const std::filesystem::path& binary_root,
                                                            const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path roadmap_path = source_root / "docs" / "plan" / "admin" /
                                               "speculative_roadmap_post_phase145.txt";
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase146_service_shape_consolidation.mirproj.txt";
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
                                                                 build_dir / "kernel_phase146_service_shape_consolidation_build_output.txt",
                                                                 "freestanding kernel phase146 service shape consolidation build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase146 freestanding kernel service shape consolidation build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase146BehaviorSlice(build_dir,
                                build_targets,
                                ResolveFreestandingKernelGoldenPath(source_root,
                                                                    "phase146_service_shape_consolidation.run.contains.txt"));
    ExpectPhase146MirStructureSlice(dump_targets.mir, mir_projection_path);
}

void RunFreestandingKernelPhase147IpcShapeAuditUnderRealUsage(const std::filesystem::path& source_root,
                                                              const std::filesystem::path& binary_root,
                                                              const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path roadmap_path = source_root / "docs" / "plan" / "admin" /
                                               "speculative_roadmap_post_phase145.txt";
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase147_ipc_shape_audit_under_real_usage.mirproj.txt";
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
                                                                 build_dir / "kernel_phase147_ipc_shape_audit_under_real_usage_build_output.txt",
                                                                 "freestanding kernel phase147 ipc shape audit under real usage build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase147 freestanding kernel ipc shape audit under real usage build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase147BehaviorSlice(build_dir,
                                build_targets,
                                ResolveFreestandingKernelGoldenPath(source_root,
                                                                    "phase147_ipc_shape_audit_under_real_usage.run.contains.txt"));
    ExpectPhase147MirStructureSlice(dump_targets.mir, mir_projection_path);
}

void RunFreestandingKernelShard9(const std::filesystem::path& source_root,
                                 const std::filesystem::path& binary_root,
                                 const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const auto artifacts = BuildAndRunFreestandingKernelTarget(mc_path,
                                                               common_paths.project_path,
                                                               common_paths.main_source_path,
                                                               binary_root / "kernel_build",
                                                               "kernel_shard9",
                                                               "freestanding kernel shard9 build",
                                                               "freestanding kernel shard9 run");

    ExpectTextContainsLinesFile(artifacts.run_output,
                                ResolveFreestandingKernelGoldenPath(source_root, "kernel_shard9.run.contains.txt"),
                                "shard9 freestanding kernel run should preserve the landed shard9 slices");

    static constexpr std::string_view kPhase143Objects[] = {
        "_Users_ro_dev_c_modern_kernel_src_log_service.mc.o",
    };
    static constexpr std::string_view kPhase143Selectors[] = {
        "TypeDecl kind=struct name=debug.Phase143LongLivedLogServiceAudit",
        "Function name=bootstrap_audit.build_phase143_long_lived_log_service_audit returns=[debug.Phase143LongLivedLogServiceAudit]",
        "Function name=log_service.observe_retention returns=[log_service.LogRetentionObservation]",
        "Function name=debug.validate_phase143_long_lived_log_service returns=[bool]",
    };
    static constexpr std::string_view kPhase144Objects[] = {
        "_Users_ro_dev_c_modern_kernel_src_kv_service.mc.o",
        "kernel__bootstrap_services.o",
    };
    static constexpr std::string_view kPhase144Selectors[] = {
        "TypeDecl kind=struct name=debug.Phase144StatefulKeyValueServiceFollowThroughAudit",
        "Function name=bootstrap_audit.build_phase144_stateful_key_value_service_follow_through_audit returns=[debug.Phase144StatefulKeyValueServiceFollowThroughAudit]",
        "Function name=bootstrap_services.execute_phase144_stateful_key_value_shell_command returns=[bootstrap_services.Phase142ShellCommandRouteResult]",
        "Function name=debug.validate_phase144_stateful_key_value_service_follow_through returns=[bool]",
    };
    static constexpr std::string_view kPhase145Objects[] = {
        "_Users_ro_dev_c_modern_kernel_src_kv_service.mc.o",
        "_Users_ro_dev_c_modern_kernel_src_init.mc.o",
        "kernel__bootstrap_services.o",
    };
    static constexpr std::string_view kPhase145Selectors[] = {
        "TypeDecl kind=struct name=debug.Phase145ServiceRestartFailureAndUsagePressureAudit",
        "Function name=bootstrap_audit.build_phase145_service_restart_failure_and_usage_pressure_audit returns=[debug.Phase145ServiceRestartFailureAndUsagePressureAudit]",
        "Function name=bootstrap_services.execute_phase145_service_restart_shell_command returns=[bootstrap_services.Phase142ShellCommandRouteResult]",
        "Function name=debug.validate_phase145_service_restart_failure_and_usage_pressure returns=[bool]",
    };
    static constexpr std::string_view kPhase146Objects[] = {
        "_Users_ro_dev_c_modern_kernel_src_shell_service.mc.o",
        "_Users_ro_dev_c_modern_kernel_src_log_service.mc.o",
        "_Users_ro_dev_c_modern_kernel_src_kv_service.mc.o",
        "_Users_ro_dev_c_modern_kernel_src_serial_service.mc.o",
        "_Users_ro_dev_c_modern_kernel_src_init.mc.o",
    };
    static constexpr std::string_view kPhase146Selectors[] = {
        "TypeDecl kind=struct name=debug.Phase146ServiceShapeConsolidationAudit",
        "Function name=bootstrap_audit.build_phase146_service_shape_consolidation_audit returns=[debug.Phase146ServiceShapeConsolidationAudit]",
        "Function name=serial_service.observe_composition returns=[serial_service.SerialCompositionObservation]",
        "Function name=debug.validate_phase146_service_shape_consolidation returns=[bool]",
    };
    static constexpr std::string_view kPhase147Objects[] = {
        "_Users_ro_dev_c_modern_kernel_src_shell_service.mc.o",
        "_Users_ro_dev_c_modern_kernel_src_log_service.mc.o",
        "_Users_ro_dev_c_modern_kernel_src_kv_service.mc.o",
        "_Users_ro_dev_c_modern_kernel_src_serial_service.mc.o",
        "kernel__bootstrap_services.o",
    };
    static constexpr std::string_view kPhase147Selectors[] = {
        "TypeDecl kind=struct name=debug.Phase147IpcShapeAudit",
        "Function name=bootstrap_audit.build_phase147_ipc_shape_audit returns=[debug.Phase147IpcShapeAudit]",
        "Function name=bootstrap_services.phase147_ipc_shape_workflow_result returns=[bootstrap_services.Phase147IpcShapeWorkflowResult]",
        "Function name=debug.validate_phase147_ipc_shape_audit_under_real_usage returns=[bool]",
    };

    const FreestandingKernelPhaseCheck phase_checks[] = {
        {"phase143 long-lived log-service audit", "phase143_long_lived_log_service_follow_through.run.contains.txt", std::span{kPhase143Objects}, std::span{kPhase143Selectors}, "phase143_long_lived_log_service_follow_through.mirproj.txt"},
        {"phase144 stateful key-value audit", "phase144_stateful_key_value_service_follow_through.run.contains.txt", std::span{kPhase144Objects}, std::span{kPhase144Selectors}, "phase144_stateful_key_value_service_follow_through.mirproj.txt"},
        {"phase145 restart-failure audit", "phase145_service_restart_failure_and_usage_pressure_audit.run.contains.txt", std::span{kPhase145Objects}, std::span{kPhase145Selectors}, "phase145_service_restart_failure_and_usage_pressure_audit.mirproj.txt"},
        {"phase146 service-shape audit", "phase146_service_shape_consolidation.run.contains.txt", std::span{kPhase146Objects}, std::span{kPhase146Selectors}, "phase146_service_shape_consolidation.mirproj.txt"},
        {"phase147 IPC-shape audit", "phase147_ipc_shape_audit_under_real_usage.run.contains.txt", std::span{kPhase147Objects}, std::span{kPhase147Selectors}, "phase147_ipc_shape_audit_under_real_usage.mirproj.txt"},
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
