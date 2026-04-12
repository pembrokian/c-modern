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

void ExpectPhase128BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets,
                                 const std::filesystem::path& expected_lines_path) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase128_service_death_run_output.txt",
                                                             "freestanding kernel phase128 service death run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("phase128 freestanding kernel service-death run should preserve the landed phase128 slice\n" + run_output);
    }
    ExpectTextContainsLinesFile(run_output,
                                expected_lines_path,
                                "phase128 freestanding kernel service-death run should preserve the landed phase128 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "kernel__bootstrap_audit.o")) {
        Fail("phase128 service-death audit should emit the bootstrap_audit module object");
    }
}

void ExpectPhase128MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "TypeDecl kind=struct name=debug.Phase128ServiceDeathObservationAudit",
            "Function name=bootstrap_audit.build_phase128_service_death_observation_audit returns=[debug.Phase128ServiceDeathObservationAudit]",
            "Function name=debug.validate_phase128_service_death_observation returns=[bool]",
        },
        expected_projection_path,
        "phase128 merged MIR should preserve the service-death projection");
}


void ExpectPhase129BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets,
                                 const std::filesystem::path& expected_lines_path) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase129_partial_failure_run_output.txt",
                                                             "freestanding kernel phase129 partial failure run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("phase129 freestanding kernel partial-failure run should preserve the landed phase129 slice\n" + run_output);
    }
    ExpectTextContainsLinesFile(run_output,
                                expected_lines_path,
                                "phase129 freestanding kernel partial-failure run should preserve the landed phase129 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "kernel__bootstrap_audit.o")) {
        Fail("phase129 partial-failure audit should emit the bootstrap_audit module object");
    }
}

void ExpectPhase129MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "TypeDecl kind=struct name=debug.Phase129PartialFailurePropagationAudit",
            "Function name=bootstrap_audit.build_phase129_partial_failure_propagation_audit returns=[debug.Phase129PartialFailurePropagationAudit]",
            "Function name=debug.validate_phase129_partial_failure_propagation returns=[bool]",
        },
        expected_projection_path,
        "phase129 merged MIR should preserve the partial-failure projection");
}


void ExpectPhase130BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets,
                                 const std::filesystem::path& expected_lines_path) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase130_restart_replacement_run_output.txt",
                                                             "freestanding kernel phase130 explicit restart or replacement run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("phase130 freestanding kernel explicit restart or replacement run should preserve the landed phase130 slice\n" + run_output);
    }
    ExpectTextContainsLinesFile(run_output,
                                expected_lines_path,
                                "phase130 freestanding kernel explicit restart or replacement run should preserve the landed phase130 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "kernel__bootstrap_audit.o")) {
        Fail("phase130 explicit restart or replacement audit should emit the bootstrap_audit module object");
    }
}

void ExpectPhase130MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "TypeDecl kind=struct name=debug.Phase130ExplicitRestartOrReplacementAudit",
            "Function name=bootstrap_audit.build_phase130_explicit_restart_or_replacement_audit returns=[debug.Phase130ExplicitRestartOrReplacementAudit]",
            "Function name=debug.validate_phase130_explicit_restart_or_replacement returns=[bool]",
        },
        expected_projection_path,
        "phase130 merged MIR should preserve the explicit restart-or-replacement projection");
}


void ExpectPhase131BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets,
                                 const std::filesystem::path& expected_lines_path) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase131_fan_out_composition_run_output.txt",
                                                             "freestanding kernel phase131 fan-out composition run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("phase131 freestanding kernel fan-out composition run should preserve the landed phase131 slice\n" + run_output);
    }
    ExpectTextContainsLinesFile(run_output,
                                expected_lines_path,
                                "phase131 freestanding kernel fan-out composition run should preserve the landed phase131 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "kernel__bootstrap_audit.o")) {
        Fail("phase131 fan-out composition audit should emit the bootstrap_audit module object");
    }
}

void ExpectPhase131MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "TypeDecl kind=struct name=debug.Phase131FanOutCompositionAudit",
            "Function name=bootstrap_audit.build_phase131_fan_out_composition_audit returns=[debug.Phase131FanOutCompositionAudit]",
            "Function name=debug.validate_phase131_fan_out_composition returns=[bool]",
        },
        expected_projection_path,
        "phase131 merged MIR should preserve the fan-out composition projection");
}


void ExpectPhase132BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets,
                                 const std::filesystem::path& expected_lines_path) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase132_backpressure_run_output.txt",
                                                             "freestanding kernel phase132 backpressure and blocking run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("phase132 freestanding kernel backpressure and blocking run should preserve the landed phase132 slice\n" + run_output);
    }
    ExpectTextContainsLinesFile(run_output,
                                expected_lines_path,
                                "phase132 freestanding kernel backpressure and blocking run should preserve the landed phase132 slice");
}

void ExpectPhase132MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "TypeDecl kind=struct name=debug.Phase132BackpressureAudit",
            "Function name=syscall.perform_send_with_backpressure returns=[syscall.BackpressureSendResult]",
            "Function name=syscall.perform_receive_with_backpressure returns=[syscall.BackpressureReceiveResult]",
            "Function name=debug.validate_phase132_backpressure_and_blocking returns=[bool]",
        },
        expected_projection_path,
        "phase132 merged MIR should preserve the backpressure projection");
}


}  // namespace

void RunFreestandingKernelPhase128ServiceDeathObservation(const std::filesystem::path& source_root,
                                                          const std::filesystem::path& binary_root,
                                                          const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase128_service_death_observation.mirproj.txt";
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
                                                                 build_dir / "kernel_phase128_service_death_build_output.txt",
                                                                 "freestanding kernel phase128 service death build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase128 freestanding kernel service-death build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase128BehaviorSlice(build_dir,
                                build_targets,
                                ResolveFreestandingKernelGoldenPath(source_root,
                                                                    "phase128_service_death_observation.run.contains.txt"));
    ExpectPhase128MirStructureSlice(dump_targets.mir, mir_projection_path);
}

void RunFreestandingKernelPhase129PartialFailurePropagation(const std::filesystem::path& source_root,
                                                            const std::filesystem::path& binary_root,
                                                            const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase129_partial_failure_propagation.mirproj.txt";
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
                                                                 build_dir / "kernel_phase129_partial_failure_build_output.txt",
                                                                 "freestanding kernel phase129 partial failure build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase129 freestanding kernel partial-failure build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase129BehaviorSlice(build_dir,
                                build_targets,
                                ResolveFreestandingKernelGoldenPath(source_root,
                                                                    "phase129_partial_failure_propagation.run.contains.txt"));
    ExpectPhase129MirStructureSlice(dump_targets.mir, mir_projection_path);
}

void RunFreestandingKernelPhase130ExplicitRestartOrReplacement(const std::filesystem::path& source_root,
                                                               const std::filesystem::path& binary_root,
                                                               const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase130_explicit_restart_or_replacement.mirproj.txt";
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
                                                                 build_dir / "kernel_phase130_restart_replacement_build_output.txt",
                                                                 "freestanding kernel phase130 explicit restart or replacement build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase130 freestanding kernel explicit restart or replacement build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase130BehaviorSlice(build_dir,
                                build_targets,
                                ResolveFreestandingKernelGoldenPath(source_root,
                                                                    "phase130_explicit_restart_or_replacement.run.contains.txt"));
    ExpectPhase130MirStructureSlice(dump_targets.mir, mir_projection_path);
}

void RunFreestandingKernelPhase131FanInOrFanOutComposition(const std::filesystem::path& source_root,
                                                           const std::filesystem::path& binary_root,
                                                           const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase131_fan_in_or_fan_out_composition.mirproj.txt";
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
                                                                 build_dir / "kernel_phase131_fan_out_composition_build_output.txt",
                                                                 "freestanding kernel phase131 fan-out composition build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase131 freestanding kernel fan-out composition build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase131BehaviorSlice(build_dir,
                                build_targets,
                                ResolveFreestandingKernelGoldenPath(source_root,
                                                                    "phase131_fan_in_or_fan_out_composition.run.contains.txt"));
    ExpectPhase131MirStructureSlice(dump_targets.mir, mir_projection_path);
}

void RunFreestandingKernelPhase132BackpressureAndBlocking(const std::filesystem::path& source_root,
                                                          const std::filesystem::path& binary_root,
                                                          const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase132_backpressure_and_blocking.mirproj.txt";
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
                                                                 build_dir / "kernel_phase132_backpressure_build_output.txt",
                                                                 "freestanding kernel phase132 backpressure and blocking build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase132 freestanding kernel backpressure and blocking build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase132BehaviorSlice(build_dir,
                                build_targets,
                                ResolveFreestandingKernelGoldenPath(source_root,
                                                                    "phase132_backpressure_and_blocking.run.contains.txt"));
    ExpectPhase132MirStructureSlice(dump_targets.mir, mir_projection_path);
}

void RunFreestandingKernelShard6(const std::filesystem::path& source_root,
                                 const std::filesystem::path& binary_root,
                                 const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const auto artifacts = BuildAndRunFreestandingKernelTarget(mc_path,
                                                               common_paths.project_path,
                                                               common_paths.main_source_path,
                                                               binary_root / "kernel_build",
                                                               "kernel_shard6",
                                                               "freestanding kernel shard6 build",
                                                               "freestanding kernel shard6 run");

    ExpectTextContainsLinesFile(artifacts.run_output,
                                ResolveFreestandingKernelGoldenPath(source_root, "kernel_shard6.run.contains.txt"),
                                "shard6 freestanding kernel run should preserve the landed shard6 slices");

    static constexpr std::string_view kBootstrapAuditObject[] = {
        "kernel__bootstrap_audit.o",
    };
    static constexpr std::string_view kPhase128Selectors[] = {
        "TypeDecl kind=struct name=debug.Phase128ServiceDeathObservationAudit",
        "Function name=bootstrap_audit.build_phase128_service_death_observation_audit returns=[debug.Phase128ServiceDeathObservationAudit]",
        "Function name=debug.validate_phase128_service_death_observation returns=[bool]",
    };
    static constexpr std::string_view kPhase129Selectors[] = {
        "TypeDecl kind=struct name=debug.Phase129PartialFailurePropagationAudit",
        "Function name=bootstrap_audit.build_phase129_partial_failure_propagation_audit returns=[debug.Phase129PartialFailurePropagationAudit]",
        "Function name=debug.validate_phase129_partial_failure_propagation returns=[bool]",
    };
    static constexpr std::string_view kPhase130Selectors[] = {
        "TypeDecl kind=struct name=debug.Phase130ExplicitRestartOrReplacementAudit",
        "Function name=bootstrap_audit.build_phase130_explicit_restart_or_replacement_audit returns=[debug.Phase130ExplicitRestartOrReplacementAudit]",
        "Function name=debug.validate_phase130_explicit_restart_or_replacement returns=[bool]",
    };
    static constexpr std::string_view kPhase131Selectors[] = {
        "TypeDecl kind=struct name=debug.Phase131FanOutCompositionAudit",
        "Function name=bootstrap_audit.build_phase131_fan_out_composition_audit returns=[debug.Phase131FanOutCompositionAudit]",
        "Function name=debug.validate_phase131_fan_out_composition returns=[bool]",
    };
    static constexpr std::string_view kPhase132Selectors[] = {
        "TypeDecl kind=struct name=debug.Phase132BackpressureAudit",
        "Function name=syscall.perform_send_with_backpressure returns=[syscall.BackpressureSendResult]",
        "Function name=syscall.perform_receive_with_backpressure returns=[syscall.BackpressureReceiveResult]",
        "Function name=debug.validate_phase132_backpressure_and_blocking returns=[bool]",
    };

    const FreestandingKernelPhaseCheck phase_checks[] = {
        {"phase128 service-death audit", "phase128_service_death_observation.run.contains.txt", std::span{kBootstrapAuditObject}, std::span{kPhase128Selectors}, "phase128_service_death_observation.mirproj.txt"},
        {"phase129 partial-failure audit", "phase129_partial_failure_propagation.run.contains.txt", std::span{kBootstrapAuditObject}, std::span{kPhase129Selectors}, "phase129_partial_failure_propagation.mirproj.txt"},
        {"phase130 restart-or-replacement audit", "phase130_explicit_restart_or_replacement.run.contains.txt", std::span{kBootstrapAuditObject}, std::span{kPhase130Selectors}, "phase130_explicit_restart_or_replacement.mirproj.txt"},
        {"phase131 fan-out composition audit", "phase131_fan_in_or_fan_out_composition.run.contains.txt", std::span{kBootstrapAuditObject}, std::span{kPhase131Selectors}, "phase131_fan_in_or_fan_out_composition.mirproj.txt"},
        {"phase132 backpressure audit", "phase132_backpressure_and_blocking.run.contains.txt", std::span<const std::string_view>{}, std::span{kPhase132Selectors}, "phase132_backpressure_and_blocking.mirproj.txt"},
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
