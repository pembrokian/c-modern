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
                                 const mc::support::BuildArtifactTargets& build_targets) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase122_target_surface_run_output.txt",
                                                             "freestanding kernel phase122 target-surface audit run");
    ExpectExitCodeAtLeast(run_outcome,
                          122,
                          run_output,
                          "phase122 freestanding kernel target-surface run should preserve the landed phase122 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "kernel__bootstrap_audit.o")) {
        Fail("phase122 target-surface audit should emit the bootstrap_audit module object");
    }
}

void ExpectPhase122PublicationSlice(const std::filesystem::path& phase_doc_path,
                                    const std::filesystem::path& support_note_path,
                                    const std::filesystem::path& roadmap_path,
                                    const std::filesystem::path& kernel_readme_path,
                                    const std::filesystem::path& repo_map_path,
                                    const std::filesystem::path& freestanding_readme_path,
                                    const std::filesystem::path& decision_log_path,
                                    const std::filesystem::path& backlog_path) {
    const std::string phase_doc = ReadFile(phase_doc_path);
    ExpectOutputContains(phase_doc,
                         "Phase 122 -- Target-Surface Audit",
                         "phase122 plan note should exist in canonical phase-doc style");
    ExpectOutputContains(phase_doc,
                         "Keep the compiler, sema, MIR, backend, ABI, and `hal` surfaces closed.",
                         "phase122 plan note should record the decision not to widen compiler work");
    ExpectOutputContains(phase_doc,
                         "Phase 122 is complete as a target-surface audit.",
                         "phase122 plan note should close out the target-surface boundary explicitly");

    const std::string support_note = ReadFile(support_note_path);
    ExpectOutputContains(support_note,
                         "arm64 bootstrap target family only",
                         "phase122 support note should keep the admitted target family unchanged");

    const std::string roadmap = ReadFile(roadmap_path);
    ExpectOutputContains(roadmap,
                         "Phase 122 is now concrete as one target-surface audit",
                         "phase122 roadmap should record the landed target boundary");

    const std::string kernel_readme = ReadFile(kernel_readme_path);
    ExpectOutputContains(kernel_readme,
                         "bounded target-surface audit",
                         "phase122 kernel README should describe the new target boundary");

    const std::string repo_map = ReadFile(repo_map_path);
    ExpectOutputContains(repo_map,
                         "phase122_target_surface_audit.cpp",
                         "phase122 repository map should list the new kernel proof owner");

    const std::string freestanding_readme = ReadFile(freestanding_readme_path);
    ExpectOutputContains(freestanding_readme,
                         "phase122_target_surface_audit.cpp",
                         "phase122 freestanding README should list the new kernel proof owner");

    const std::string decision_log = ReadFile(decision_log_path);
    ExpectOutputContains(decision_log,
                         "Phase 122 target-surface audit",
                         "phase122 decision log should record the limited-scope target publication decision");

    const std::string backlog = ReadFile(backlog_path);
    ExpectOutputContains(backlog,
                         "keep the Phase 122 target-surface audit explicit",
                         "phase122 backlog should keep the new target boundary visible for later phases");
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

}  // namespace

void RunFreestandingKernelPhase122TargetSurfaceAudit(const std::filesystem::path& source_root,
                                                     const std::filesystem::path& binary_root,
                                                     const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path phase_doc_path = source_root / "docs" / "plan" /
                                                 "phase122_target_surface_audit.txt";
    const std::filesystem::path support_note_path = source_root / "docs" / "plan" /
                                                    "canopus_running_system_support_statement.txt";
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase122_target_surface_audit.mirproj.txt";
    const std::filesystem::path build_dir = binary_root / "kernel_phase122_target_surface_build";
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
                                                                 build_dir / "kernel_phase122_target_surface_build_output.txt",
                                                                 "freestanding kernel phase122 target-surface audit build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase122 freestanding kernel target-surface audit build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase122BehaviorSlice(build_dir, build_targets);
    ExpectPhase122PublicationSlice(phase_doc_path,
                                   support_note_path,
                                   common_paths.roadmap_path,
                                   common_paths.kernel_readme_path,
                                   common_paths.repo_map_path,
                                   common_paths.freestanding_readme_path,
                                   common_paths.decision_log_path,
                                   common_paths.backlog_path);
    ExpectPhase122MirStructureSlice(dump_targets.mir, mir_projection_path);
}

}  // namespace mc::tool_tests
