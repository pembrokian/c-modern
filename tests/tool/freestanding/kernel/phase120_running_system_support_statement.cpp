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

void ExpectPhase120BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase120_support_run_output.txt",
                                                             "freestanding kernel phase120 running-system support run");
    ExpectExitCodeAtLeast(run_outcome,
                          120,
                          run_output,
                          "phase120 freestanding kernel running-system support run should preserve the landed phase120 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "kernel__bootstrap_audit.o")) {
        Fail("phase120 running-system support audit should emit the bootstrap_audit module object");
    }
}

void ExpectPhase120PublicationSlice(const std::filesystem::path& phase_doc_path,
                                    const std::filesystem::path& support_note_path,
                                    const std::filesystem::path& roadmap_path,
                                    const std::filesystem::path& kernel_readme_path,
                                    const std::filesystem::path& repo_map_path,
                                    const std::filesystem::path& freestanding_readme_path,
                                    const std::filesystem::path& decision_log_path,
                                    const std::filesystem::path& backlog_path) {
    const std::string phase_doc = ReadFile(phase_doc_path);
    ExpectOutputContains(phase_doc,
                         "Phase 120 -- Running-System Support Statement",
                         "phase120 plan note should exist in canonical phase-doc style");
    ExpectOutputContains(phase_doc,
                         "Keep the compiler, sema, MIR, backend, ABI, and `hal` surfaces closed.",
                         "phase120 plan note should record the decision not to widen compiler work");
    ExpectOutputContains(phase_doc,
                         "Phase 120 is complete as a running-system support statement.",
                         "phase120 plan note should close out the running-system support boundary explicitly");

    const std::string support_note = ReadFile(support_note_path);
    ExpectOutputContains(support_note,
                         "Canopus Running-System Support Statement",
                         "phase120 support note should exist in canonical support-note style");
    ExpectOutputContains(support_note,
                         "The current repository can honestly claim one bounded running-system support statement",
                         "phase120 support note should publish the admitted running-system slice explicitly");

    const std::string roadmap = ReadFile(roadmap_path);
    ExpectOutputContains(roadmap,
                         "Phase 120 is now concrete as one running-system support statement",
                         "phase120 roadmap should record the landed running-system support boundary");

    const std::string kernel_readme = ReadFile(kernel_readme_path);
    ExpectOutputContains(kernel_readme,
                         "bounded running-system support statement",
                         "phase120 kernel README should describe the new support boundary");

    const std::string repo_map = ReadFile(repo_map_path);
    ExpectOutputContains(repo_map,
                         "phase120_running_system_support_statement.cpp",
                         "phase120 repository map should list the new kernel proof owner");

    const std::string freestanding_readme = ReadFile(freestanding_readme_path);
    ExpectOutputContains(freestanding_readme,
                         "phase120_running_system_support_statement.cpp",
                         "phase120 freestanding README should list the new kernel proof owner");

    const std::string decision_log = ReadFile(decision_log_path);
    ExpectOutputContains(decision_log,
                         "Phase 120 running-system support statement",
                         "phase120 decision log should record the limited-scope running-system publication decision");

    const std::string backlog = ReadFile(backlog_path);
    ExpectOutputContains(backlog,
                         "keep the Phase 120 running-system support statement explicit",
                         "phase120 backlog should keep the new support boundary visible for later phases");
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

}  // namespace

void RunFreestandingKernelPhase120RunningSystemSupportStatement(const std::filesystem::path& source_root,
                                                                const std::filesystem::path& binary_root,
                                                                const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path phase_doc_path = ResolvePlanDocPath(source_root,
                                                                    "phase120_running_system_support_statement.txt");
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
    ExpectPhase120BehaviorSlice(build_dir, build_targets);
    ExpectPhase120PublicationSlice(phase_doc_path,
                                   support_note_path,
                                   ResolveCanopusRoadmapPath(source_root, 120),
                                   common_paths.kernel_readme_path,
                                   common_paths.repo_map_path,
                                   common_paths.freestanding_readme_path,
                                   common_paths.decision_log_path,
                                   common_paths.backlog_path);
    ExpectPhase120MirStructureSlice(dump_targets.mir, mir_projection_path);
}

}  // namespace mc::tool_tests