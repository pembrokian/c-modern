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

void ExpectPhase124BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase124_delegation_chain_run_output.txt",
                                                             "freestanding kernel phase124 delegation-chain run");
    ExpectExitCodeAtLeast(run_outcome,
                          124,
                          run_output,
                          "phase124 freestanding kernel delegation-chain run should preserve the landed phase124 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "kernel__bootstrap_audit.o")) {
        Fail("phase124 delegation-chain stress audit should emit the bootstrap_audit module object");
    }
}

void ExpectPhase124PublicationSlice(const std::filesystem::path& phase_doc_path,
                                    const std::filesystem::path& roadmap_path,
                                    const std::filesystem::path& position_path,
                                    const std::filesystem::path& kernel_readme_path,
                                    const std::filesystem::path& repo_map_path,
                                    const std::filesystem::path& freestanding_readme_path,
                                    const std::filesystem::path& decision_log_path,
                                    const std::filesystem::path& backlog_path) {
    const std::string phase_doc = ReadFile(phase_doc_path);
    ExpectOutputContains(phase_doc,
                         "Phase 124 -- Delegation Chain Stress",
                         "phase124 plan note should exist in canonical phase-doc style");
    ExpectOutputContains(phase_doc,
                         "Keep the compiler, sema, MIR, backend, ABI, and `hal` surfaces closed.",
                         "phase124 plan note should record the decision not to widen compiler work");
    ExpectOutputContains(phase_doc,
                         "Phase 124 is complete as a delegation-chain stress step.",
                         "phase124 plan note should close out the delegation-chain boundary explicitly");

    const std::string roadmap = ReadFile(roadmap_path);
    ExpectOutputContains(roadmap,
                         "Phase 124 is now concrete as one delegation-chain stress step",
                         "phase124 roadmap should record the landed authority-chain boundary");

    const std::string position = ReadFile(position_path);
    ExpectOutputContains(position,
                         "landed Phase 124 delegation-chain stress step",
                         "phase124 position note should record the landed authority-chain step");

    const std::string kernel_readme = ReadFile(kernel_readme_path);
    ExpectOutputContains(kernel_readme,
                         "bounded delegation-chain stress step",
                         "phase124 kernel README should describe the new authority-chain boundary");

    const std::string repo_map = ReadFile(repo_map_path);
    ExpectOutputContains(repo_map,
                         "phase124_delegation_chain_stress.cpp",
                         "phase124 repository map should list the new kernel proof owner");

    const std::string freestanding_readme = ReadFile(freestanding_readme_path);
    ExpectOutputContains(freestanding_readme,
                         "phase124_delegation_chain_stress.cpp",
                         "phase124 freestanding README should list the new kernel proof owner");

    const std::string decision_log = ReadFile(decision_log_path);
    ExpectOutputContains(decision_log,
                         "Phase 124 delegation-chain stress",
                         "phase124 decision log should record the limited-scope delegation decision");

    const std::string backlog = ReadFile(backlog_path);
    ExpectOutputContains(backlog,
                         "keep the Phase 124 delegation-chain stress explicit",
                         "phase124 backlog should keep the new authority-chain boundary visible for later phases");
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

}  // namespace

void RunFreestandingKernelPhase124DelegationChainStress(const std::filesystem::path& source_root,
                                                        const std::filesystem::path& binary_root,
                                                        const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path phase_doc_path = ResolvePlanDocPath(source_root,
                                                                    "phase124_delegation_chain_stress.txt");
    const std::filesystem::path position_path = source_root / "docs" / "plan" / "admin" /
                                                "modern_c_canopus_readiness_position.txt";
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase124_delegation_chain_stress.mirproj.txt";
    const std::filesystem::path build_dir = binary_root / "kernel_phase124_delegation_chain_build";
    std::filesystem::remove_all(build_dir);

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
    ExpectPhase124BehaviorSlice(build_dir, build_targets);
    ExpectPhase124PublicationSlice(phase_doc_path,
                                   common_paths.roadmap_path,
                                   position_path,
                                   common_paths.kernel_readme_path,
                                   common_paths.repo_map_path,
                                   common_paths.freestanding_readme_path,
                                   common_paths.decision_log_path,
                                   common_paths.backlog_path);
    ExpectPhase124MirStructureSlice(dump_targets.mir, mir_projection_path);
}

}  // namespace mc::tool_tests