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

void ExpectPhase140BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets,
                                 const std::filesystem::path& expected_lines_path) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase140_serial_ingress_composed_service_graph_run_output.txt",
                                                             "freestanding kernel phase140 serial-ingress composed service graph run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("phase140 freestanding kernel serial-ingress composed service graph run should preserve the landed phase140 slice\n" + run_output);
    }
    ExpectTextContainsLinesFile(run_output,
                                expected_lines_path,
                                "phase140 freestanding kernel serial-ingress composed service graph run should preserve the landed phase140 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_serial_service.mc.o")) {
        Fail("phase140 serial-ingress composed service graph should emit the serial_service module object");
    }
    if (!std::filesystem::exists(object_dir / "kernel__bootstrap_audit.o")) {
        Fail("phase140 serial-ingress composed service graph should emit the bootstrap_audit module object");
    }
}

void ExpectPhase140MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "TypeDecl kind=struct name=debug.Phase140SerialIngressComposedServiceGraphAudit",
            "Function name=bootstrap_audit.build_phase140_serial_ingress_composed_service_graph_audit returns=[debug.Phase140SerialIngressComposedServiceGraphAudit]",
            "Function name=bootstrap_services.execute_phase140_serial_ingress_composed_service_graph returns=[bootstrap_services.Phase140SerialIngressCompositionResult]",
            "Function name=debug.validate_phase140_serial_ingress_composed_service_graph returns=[bool]",
        },
        expected_projection_path,
        "phase140 merged MIR should preserve the serial-ingress composed graph projection");
}


void ExpectPhase141BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets,
                                 const std::filesystem::path& expected_lines_path) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase141_interactive_service_system_scope_freeze_run_output.txt",
                                                             "freestanding kernel phase141 interactive service system scope freeze run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("phase141 freestanding kernel interactive service system scope freeze run should preserve the landed phase141 slice\n" + run_output);
    }
    ExpectTextContainsLinesFile(run_output,
                                expected_lines_path,
                                "phase141 freestanding kernel interactive service system scope freeze run should preserve the landed phase141 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_shell_service.mc.o")) {
        Fail("phase141 interactive service system scope freeze should emit the shell_service module object");
    }
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_kv_service.mc.o")) {
        Fail("phase141 interactive service system scope freeze should emit the kv_service module object");
    }
}

void ExpectPhase141MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "TypeDecl kind=struct name=debug.Phase141InteractiveServiceSystemScopeFreezeAudit",
            "Function name=bootstrap_audit.build_phase141_interactive_service_system_scope_freeze_audit returns=[debug.Phase141InteractiveServiceSystemScopeFreezeAudit]",
            "Function name=debug.validate_phase141_interactive_service_system_scope_freeze returns=[bool]",
        },
        expected_projection_path,
        "phase141 merged MIR should preserve the interactive service system scope-freeze projection");
}


void ExpectPhase142BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets,
                                 const std::filesystem::path& expected_lines_path) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase142_serial_shell_command_routing_run_output.txt",
                                                             "freestanding kernel phase142 serial shell command routing run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("phase142 freestanding kernel serial shell command routing run should preserve the landed phase142 slice\n" + run_output);
    }
    ExpectTextContainsLinesFile(run_output,
                                expected_lines_path,
                                "phase142 freestanding kernel serial shell command routing run should preserve the landed phase142 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_shell_service.mc.o")) {
        Fail("phase142 serial shell command routing should emit the shell_service module object");
    }
    if (!std::filesystem::exists(object_dir / "kernel__bootstrap_services.o")) {
        Fail("phase142 serial shell command routing should emit the bootstrap_services module object");
    }
}

void ExpectPhase142MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "TypeDecl kind=struct name=debug.Phase142SerialShellCommandRoutingAudit",
            "Function name=bootstrap_audit.build_phase142_serial_shell_command_routing_audit returns=[debug.Phase142SerialShellCommandRoutingAudit]",
            "Function name=bootstrap_services.execute_phase142_shell_command returns=[bootstrap_services.Phase142ShellCommandRouteResult]",
            "Function name=debug.validate_phase142_serial_shell_command_routing returns=[bool]",
        },
        expected_projection_path,
        "phase142 merged MIR should preserve the serial shell command-routing projection");
}


}  // namespace

void RunFreestandingKernelPhase140SerialIngressComposedServiceGraph(const std::filesystem::path& source_root,
                                                                    const std::filesystem::path& binary_root,
                                                                    const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase140_serial_ingress_composed_service_graph.mirproj.txt";
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
                                                                 build_dir / "kernel_phase140_serial_ingress_composed_service_graph_build_output.txt",
                                                                 "freestanding kernel phase140 serial-ingress composed service graph build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase140 freestanding kernel serial-ingress composed service graph build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase140BehaviorSlice(build_dir,
                                build_targets,
                                ResolveFreestandingKernelGoldenPath(source_root,
                                                                    "phase140_serial_ingress_composed_service_graph.run.contains.txt"));
    ExpectPhase140MirStructureSlice(dump_targets.mir, mir_projection_path);
}

void RunFreestandingKernelPhase141InteractiveServiceSystemScopeFreeze(const std::filesystem::path& source_root,
                                                                     const std::filesystem::path& binary_root,
                                                                     const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase141_interactive_service_system_scope_freeze.mirproj.txt";
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
                                                                 build_dir / "kernel_phase141_interactive_service_system_scope_freeze_build_output.txt",
                                                                 "freestanding kernel phase141 interactive service system scope freeze build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase141 freestanding kernel interactive service system scope freeze build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase141BehaviorSlice(build_dir,
                                build_targets,
                                ResolveFreestandingKernelGoldenPath(source_root,
                                                                    "phase141_interactive_service_system_scope_freeze.run.contains.txt"));
    ExpectPhase141MirStructureSlice(dump_targets.mir, mir_projection_path);
}

void RunFreestandingKernelPhase142SerialShellCommandRouting(const std::filesystem::path& source_root,
                                                            const std::filesystem::path& binary_root,
                                                            const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase142_serial_shell_command_routing.mirproj.txt";
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
                                                                 build_dir / "kernel_phase142_serial_shell_command_routing_build_output.txt",
                                                                 "freestanding kernel phase142 serial shell command routing build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase142 freestanding kernel serial shell command routing build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase142BehaviorSlice(build_dir,
                                build_targets,
                                ResolveFreestandingKernelGoldenPath(source_root,
                                                                    "phase142_serial_shell_command_routing.run.contains.txt"));
    ExpectPhase142MirStructureSlice(dump_targets.mir, mir_projection_path);
}

void RunFreestandingKernelShard8(const std::filesystem::path& source_root,
                                 const std::filesystem::path& binary_root,
                                 const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const auto artifacts = BuildAndRunFreestandingKernelTarget(mc_path,
                                                               common_paths.project_path,
                                                               common_paths.main_source_path,
                                                               binary_root / "kernel_build",
                                                               "kernel_shard8",
                                                               "freestanding kernel shard8 build",
                                                               "freestanding kernel shard8 run");

    ExpectTextContainsLinesFile(artifacts.run_output,
                                ResolveFreestandingKernelGoldenPath(source_root, "kernel_shard8.run.contains.txt"),
                                "shard8 freestanding kernel run should preserve the landed shard8 slices");

    static constexpr std::string_view kPhase140Objects[] = {
        "_Users_ro_dev_c_modern_kernel_src_serial_service.mc.o",
        "kernel__bootstrap_audit.o",
    };
    static constexpr std::string_view kPhase140Selectors[] = {
        "TypeDecl kind=struct name=debug.Phase140SerialIngressComposedServiceGraphAudit",
        "Function name=bootstrap_audit.build_phase140_serial_ingress_composed_service_graph_audit returns=[debug.Phase140SerialIngressComposedServiceGraphAudit]",
        "Function name=bootstrap_services.execute_phase140_serial_ingress_composed_service_graph returns=[bootstrap_services.Phase140SerialIngressCompositionResult]",
        "Function name=debug.validate_phase140_serial_ingress_composed_service_graph returns=[bool]",
    };
    static constexpr std::string_view kPhase141Objects[] = {
        "_Users_ro_dev_c_modern_kernel_src_shell_service.mc.o",
        "_Users_ro_dev_c_modern_kernel_src_kv_service.mc.o",
    };
    static constexpr std::string_view kPhase141Selectors[] = {
        "TypeDecl kind=struct name=debug.Phase141InteractiveServiceSystemScopeFreezeAudit",
        "Function name=bootstrap_audit.build_phase141_interactive_service_system_scope_freeze_audit returns=[debug.Phase141InteractiveServiceSystemScopeFreezeAudit]",
        "Function name=debug.validate_phase141_interactive_service_system_scope_freeze returns=[bool]",
    };
    static constexpr std::string_view kPhase142Objects[] = {
        "_Users_ro_dev_c_modern_kernel_src_shell_service.mc.o",
        "kernel__bootstrap_services.o",
    };
    static constexpr std::string_view kPhase142Selectors[] = {
        "TypeDecl kind=struct name=debug.Phase142SerialShellCommandRoutingAudit",
        "Function name=bootstrap_audit.build_phase142_serial_shell_command_routing_audit returns=[debug.Phase142SerialShellCommandRoutingAudit]",
        "Function name=bootstrap_services.execute_phase142_shell_command returns=[bootstrap_services.Phase142ShellCommandRouteResult]",
        "Function name=debug.validate_phase142_serial_shell_command_routing returns=[bool]",
    };

    const FreestandingKernelPhaseCheck phase_checks[] = {
        {"phase140 serial-ingress graph audit", "phase140_serial_ingress_composed_service_graph.run.contains.txt", std::span{kPhase140Objects}, std::span{kPhase140Selectors}, "phase140_serial_ingress_composed_service_graph.mirproj.txt"},
        {"phase141 service scope-freeze audit", "phase141_interactive_service_system_scope_freeze.run.contains.txt", std::span{kPhase141Objects}, std::span{kPhase141Selectors}, "phase141_interactive_service_system_scope_freeze.mirproj.txt"},
        {"phase142 shell-routing audit", "phase142_serial_shell_command_routing.run.contains.txt", std::span{kPhase142Objects}, std::span{kPhase142Selectors}, "phase142_serial_shell_command_routing.mirproj.txt"},
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
