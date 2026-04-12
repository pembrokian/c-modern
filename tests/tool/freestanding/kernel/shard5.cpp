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

void ExpectPhase122BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets,
                                 const std::filesystem::path& expected_lines_path) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase122_target_surface_run_output.txt",
                                                             "freestanding kernel phase122 target-surface audit run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("phase122 freestanding kernel target-surface run should preserve the landed phase122 slice\n" + run_output);
    }
    ExpectTextContainsLinesFile(run_output,
                                expected_lines_path,
                                "phase122 freestanding kernel target-surface run should preserve the landed phase122 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "kernel__bootstrap_audit.o")) {
        Fail("phase122 target-surface audit should emit the bootstrap_audit module object");
    }
}

void ExpectPhase122MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "TypeDecl kind=struct name=debug.Phase122TargetSurfaceAudit",
            "Function name=bootstrap_audit.build_phase122_target_surface_audit returns=[debug.Phase122TargetSurfaceAudit]",
            "Function name=debug.validate_phase122_target_surface_audit returns=[bool]",
        },
        expected_projection_path,
        "phase122 merged MIR should preserve the target-surface projection");
}


void ExpectPhase123BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets,
                                 const std::filesystem::path& expected_lines_path) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase123_next_plateau_run_output.txt",
                                                             "freestanding kernel phase123 next-plateau audit run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("phase123 freestanding kernel next-plateau run should preserve the landed phase123 slice\n" + run_output);
    }
    ExpectTextContainsLinesFile(run_output,
                                expected_lines_path,
                                "phase123 freestanding kernel next-plateau run should preserve the landed phase123 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "kernel__bootstrap_audit.o")) {
        Fail("phase123 next-plateau audit should emit the bootstrap_audit module object");
    }
}

void ExpectPhase123MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "TypeDecl kind=struct name=debug.Phase123NextPlateauAudit",
            "Function name=bootstrap_audit.build_phase123_next_plateau_audit returns=[debug.Phase123NextPlateauAudit]",
            "Function name=debug.validate_phase123_next_plateau_audit returns=[bool]",
        },
        expected_projection_path,
        "phase123 merged MIR should preserve the next-plateau projection");
}


void ExpectPhase124BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets,
                                 const std::filesystem::path& expected_lines_path) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase124_delegation_chain_run_output.txt",
                                                             "freestanding kernel phase124 delegation-chain run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("phase124 freestanding kernel delegation-chain run should preserve the landed phase124 slice\n" + run_output);
    }
    ExpectTextContainsLinesFile(run_output,
                                expected_lines_path,
                                "phase124 freestanding kernel delegation-chain run should preserve the landed phase124 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "kernel__bootstrap_audit.o")) {
        Fail("phase124 delegation-chain stress audit should emit the bootstrap_audit module object");
    }
}

void ExpectPhase124MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "TypeDecl kind=struct name=debug.Phase124DelegationChainAudit",
            "Function name=bootstrap_audit.build_phase124_delegation_chain_audit returns=[debug.Phase124DelegationChainAudit]",
            "Function name=debug.validate_phase124_delegation_chain_stress returns=[bool]",
        },
        expected_projection_path,
        "phase124 merged MIR should preserve the delegation-chain projection");
}


void ExpectPhase125BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets,
                                 const std::filesystem::path& expected_lines_path) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase125_invalidation_run_output.txt",
                                                             "freestanding kernel phase125 invalidation run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("phase125 freestanding kernel invalidation run should preserve the landed phase125 slice\n" + run_output);
    }
    ExpectTextContainsLinesFile(run_output,
                                expected_lines_path,
                                "phase125 freestanding kernel invalidation run should preserve the landed phase125 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "kernel__bootstrap_audit.o")) {
        Fail("phase125 invalidation audit should emit the bootstrap_audit module object");
    }
}

void ExpectPhase125MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "TypeDecl kind=struct name=debug.Phase125InvalidationAudit",
            "Function name=bootstrap_audit.build_phase125_invalidation_audit returns=[debug.Phase125InvalidationAudit]",
            "Function name=debug.validate_phase125_invalidation_and_rejection_audit returns=[bool]",
        },
        expected_projection_path,
        "phase125 merged MIR should preserve the invalidation projection");
}


void ExpectPhase126BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets,
                                 const std::filesystem::path& expected_lines_path) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase126_lifetime_run_output.txt",
                                                             "freestanding kernel phase126 authority lifetime run");
    if (!run_outcome.exited || run_outcome.exit_code != 0) {
        Fail("phase126 freestanding kernel authority-lifetime run should preserve the landed phase126 slice\n" + run_output);
    }
    ExpectTextContainsLinesFile(run_output,
                                expected_lines_path,
                                "phase126 freestanding kernel authority-lifetime run should preserve the landed phase126 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "kernel__bootstrap_audit.o")) {
        Fail("phase126 authority lifetime audit should emit the bootstrap_audit module object");
    }
}

void ExpectPhase126MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "TypeDecl kind=struct name=debug.Phase126AuthorityLifetimeAudit",
            "Function name=bootstrap_audit.build_phase126_authority_lifetime_audit returns=[debug.Phase126AuthorityLifetimeAudit]",
            "Function name=debug.validate_phase126_authority_lifetime_classification returns=[bool]",
        },
        expected_projection_path,
        "phase126 merged MIR should preserve the authority lifetime projection");
}


}  // namespace

void RunFreestandingKernelPhase122TargetSurfaceAudit(const std::filesystem::path& source_root,
                                                     const std::filesystem::path& binary_root,
                                                     const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path support_note_path = source_root / "docs" / "plan" /
                                                    "canopus_running_system_support_statement.txt";
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase122_target_surface_audit.mirproj.txt";
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
                                                                 build_dir / "kernel_phase122_target_surface_build_output.txt",
                                                                 "freestanding kernel phase122 target-surface audit build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase122 freestanding kernel target-surface audit build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase122BehaviorSlice(build_dir,
                                build_targets,
                                ResolveFreestandingKernelGoldenPath(source_root,
                                                                    "phase122_target_surface_audit.run.contains.txt"));
    ExpectPhase122MirStructureSlice(dump_targets.mir, mir_projection_path);
}

void RunFreestandingKernelPhase123NextPlateauAudit(const std::filesystem::path& source_root,
                                                   const std::filesystem::path& binary_root,
                                                   const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase123_next_plateau_audit.mirproj.txt";
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
                                                                 build_dir / "kernel_phase123_next_plateau_build_output.txt",
                                                                 "freestanding kernel phase123 next-plateau audit build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase123 freestanding kernel next-plateau audit build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase123BehaviorSlice(build_dir,
                                build_targets,
                                ResolveFreestandingKernelGoldenPath(source_root,
                                                                    "phase123_next_plateau_audit.run.contains.txt"));
    ExpectPhase123MirStructureSlice(dump_targets.mir, mir_projection_path);
}

void RunFreestandingKernelPhase124DelegationChainStress(const std::filesystem::path& source_root,
                                                        const std::filesystem::path& binary_root,
                                                        const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase124_delegation_chain_stress.mirproj.txt";
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
                                                                 build_dir / "kernel_phase124_delegation_chain_build_output.txt",
                                                                 "freestanding kernel phase124 delegation-chain build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase124 freestanding kernel delegation-chain build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase124BehaviorSlice(build_dir,
                                build_targets,
                                ResolveFreestandingKernelGoldenPath(source_root,
                                                                    "phase124_delegation_chain_stress.run.contains.txt"));
    ExpectPhase124MirStructureSlice(dump_targets.mir, mir_projection_path);
}

void RunFreestandingKernelPhase125InvalidationAndRejectionAudit(const std::filesystem::path& source_root,
                                                                const std::filesystem::path& binary_root,
                                                                const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase125_invalidation_and_rejection_audit.mirproj.txt";
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
                                                                 build_dir / "kernel_phase125_invalidation_build_output.txt",
                                                                 "freestanding kernel phase125 invalidation build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase125 freestanding kernel invalidation build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase125BehaviorSlice(build_dir,
                                build_targets,
                                ResolveFreestandingKernelGoldenPath(source_root,
                                                                    "phase125_invalidation_and_rejection_audit.run.contains.txt"));
    ExpectPhase125MirStructureSlice(dump_targets.mir, mir_projection_path);
}

void RunFreestandingKernelPhase126AuthorityLifetimeClassification(const std::filesystem::path& source_root,
                                                                 const std::filesystem::path& binary_root,
                                                                 const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase126_authority_lifetime_classification.mirproj.txt";
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
                                                                 build_dir / "kernel_phase126_authority_lifetime_build_output.txt",
                                                                 "freestanding kernel phase126 authority lifetime build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase126 freestanding kernel authority-lifetime build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase126BehaviorSlice(build_dir,
                                build_targets,
                                ResolveFreestandingKernelGoldenPath(source_root,
                                                                    "phase126_authority_lifetime_classification.run.contains.txt"));
    ExpectPhase126MirStructureSlice(dump_targets.mir, mir_projection_path);
}

void RunFreestandingKernelShard5(const std::filesystem::path& source_root,
                                 const std::filesystem::path& binary_root,
                                 const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const auto artifacts = BuildAndRunFreestandingKernelTarget(mc_path,
                                                               common_paths.project_path,
                                                               common_paths.main_source_path,
                                                               binary_root / "kernel_build",
                                                               "kernel_shard5",
                                                               "freestanding kernel shard5 build",
                                                               "freestanding kernel shard5 run");

    ExpectTextContainsLinesFile(artifacts.run_output,
                                ResolveFreestandingKernelGoldenPath(source_root, "kernel_shard5.run.contains.txt"),
                                "shard5 freestanding kernel run should preserve the landed shard5 slices");

    static constexpr std::string_view kBootstrapAuditObject[] = {
        "kernel__bootstrap_audit.o",
    };
    static constexpr std::string_view kPhase122Selectors[] = {
        "TypeDecl kind=struct name=debug.Phase122TargetSurfaceAudit",
        "Function name=bootstrap_audit.build_phase122_target_surface_audit returns=[debug.Phase122TargetSurfaceAudit]",
        "Function name=debug.validate_phase122_target_surface_audit returns=[bool]",
    };
    static constexpr std::string_view kPhase123Selectors[] = {
        "TypeDecl kind=struct name=debug.Phase123NextPlateauAudit",
        "Function name=bootstrap_audit.build_phase123_next_plateau_audit returns=[debug.Phase123NextPlateauAudit]",
        "Function name=debug.validate_phase123_next_plateau_audit returns=[bool]",
    };
    static constexpr std::string_view kPhase124Selectors[] = {
        "TypeDecl kind=struct name=debug.Phase124DelegationChainAudit",
        "Function name=bootstrap_audit.build_phase124_delegation_chain_audit returns=[debug.Phase124DelegationChainAudit]",
        "Function name=debug.validate_phase124_delegation_chain_stress returns=[bool]",
    };
    static constexpr std::string_view kPhase125Selectors[] = {
        "TypeDecl kind=struct name=debug.Phase125InvalidationAudit",
        "Function name=bootstrap_audit.build_phase125_invalidation_audit returns=[debug.Phase125InvalidationAudit]",
        "Function name=debug.validate_phase125_invalidation_and_rejection_audit returns=[bool]",
    };
    static constexpr std::string_view kPhase126Selectors[] = {
        "TypeDecl kind=struct name=debug.Phase126AuthorityLifetimeAudit",
        "Function name=bootstrap_audit.build_phase126_authority_lifetime_audit returns=[debug.Phase126AuthorityLifetimeAudit]",
        "Function name=debug.validate_phase126_authority_lifetime_classification returns=[bool]",
    };

    const FreestandingKernelPhaseCheck phase_checks[] = {
        {"phase122 target-surface audit", "phase122_target_surface_audit.run.contains.txt", std::span{kBootstrapAuditObject}, std::span{kPhase122Selectors}, "phase122_target_surface_audit.mirproj.txt"},
        {"phase123 next-plateau audit", "phase123_next_plateau_audit.run.contains.txt", std::span{kBootstrapAuditObject}, std::span{kPhase123Selectors}, "phase123_next_plateau_audit.mirproj.txt"},
        {"phase124 delegation-chain audit", "phase124_delegation_chain_stress.run.contains.txt", std::span{kBootstrapAuditObject}, std::span{kPhase124Selectors}, "phase124_delegation_chain_stress.mirproj.txt"},
        {"phase125 invalidation audit", "phase125_invalidation_and_rejection_audit.run.contains.txt", std::span{kBootstrapAuditObject}, std::span{kPhase125Selectors}, "phase125_invalidation_and_rejection_audit.mirproj.txt"},
        {"phase126 authority-lifetime audit", "phase126_authority_lifetime_classification.run.contains.txt", std::span{kBootstrapAuditObject}, std::span{kPhase126Selectors}, "phase126_authority_lifetime_classification.mirproj.txt"},
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
