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

void ExpectPhase125BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase125_invalidation_run_output.txt",
                                                             "freestanding kernel phase125 invalidation run");
    ExpectExitCodeAtLeast(run_outcome,
                          125,
                          run_output,
                          "phase125 freestanding kernel invalidation run should preserve the landed phase125 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "kernel__bootstrap_audit.o")) {
        Fail("phase125 invalidation audit should emit the bootstrap_audit module object");
    }
}

void ExpectPhase125PublicationSlice(const std::filesystem::path& phase_doc_path,
                                    const std::filesystem::path& roadmap_path,
                                    const std::filesystem::path& position_path,
                                    const std::filesystem::path& kernel_readme_path,
                                    const std::filesystem::path& repo_map_path,
                                    const std::filesystem::path& tool_readme_path,
                                    const std::filesystem::path& freestanding_readme_path,
                                    const std::filesystem::path& decision_log_path,
                                    const std::filesystem::path& backlog_path) {
    const std::string phase_doc = ReadFile(phase_doc_path);
    ExpectOutputContains(phase_doc,
                         "Phase 125 -- Invalidation And Rejection Audit",
                         "phase125 plan note should exist in canonical phase-doc style");
    ExpectOutputContains(phase_doc,
                         "Keep the compiler, sema, MIR, backend, ABI, and `hal` surfaces closed.",
                         "phase125 plan note should record the decision not to widen compiler work");
    ExpectOutputContains(phase_doc,
                         "Phase 125 is complete as an invalidation and rejection audit step.",
                         "phase125 plan note should close out the invalidation boundary explicitly");

    const std::string roadmap = ReadFile(roadmap_path);
    ExpectOutputContains(roadmap,
                         "Phase 125 is now concrete as one invalidation and rejection audit step",
                         "phase125 roadmap should record the landed invalidation boundary");

    const std::string position = ReadFile(position_path);
    ExpectOutputContains(position,
                         "landed Phase 125 invalidation and rejection audit step",
                         "phase125 position note should record the landed invalidation step");

    const std::string kernel_readme = ReadFile(kernel_readme_path);
    ExpectOutputContains(kernel_readme,
                         "bounded invalidation and rejection audit step",
                         "phase125 kernel README should describe the new invalidation boundary");

    const std::string repo_map = ReadFile(repo_map_path);
    ExpectOutputContains(repo_map,
                         "phase125_invalidation_and_rejection_audit.cpp",
                         "phase125 repository map should list the new kernel proof owner");

    const std::string tool_readme = ReadFile(tool_readme_path);
    ExpectOutputContains(tool_readme,
                         "kernel/phase125_invalidation_and_rejection_audit.cpp",
                         "phase125 tool README should list the new kernel proof owner");

    const std::string freestanding_readme = ReadFile(freestanding_readme_path);
    ExpectOutputContains(freestanding_readme,
                         "phase125_invalidation_and_rejection_audit.cpp",
                         "phase125 freestanding README should list the new kernel proof owner");

    const std::string decision_log = ReadFile(decision_log_path);
    ExpectOutputContains(decision_log,
                         "Phase 125 invalidation and rejection audit",
                         "phase125 decision log should record the limited-scope invalidation decision");

    const std::string backlog = ReadFile(backlog_path);
    ExpectOutputContains(backlog,
                         "keep the Phase 125 invalidation and rejection audit explicit",
                         "phase125 backlog should keep the new invalidation boundary visible for later phases");
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

}  // namespace

void RunFreestandingKernelPhase125InvalidationAndRejectionAudit(const std::filesystem::path& source_root,
                                                                const std::filesystem::path& binary_root,
                                                                const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path phase_doc_path = source_root / "docs" / "plan" /
                                                 "phase125_invalidation_and_rejection_audit.txt";
    const std::filesystem::path position_path = source_root / "docs" / "plan" / "admin" /
                                                "modern_c_canopus_readiness_position.txt";
    const std::filesystem::path tool_readme_path = source_root / "tests" / "tool" / "README.md";
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase125_invalidation_and_rejection_audit.mirproj.txt";
    const std::filesystem::path build_dir = binary_root / "kernel_phase125_invalidation_build";
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
                                                                 build_dir / "kernel_phase125_invalidation_build_output.txt",
                                                                 "freestanding kernel phase125 invalidation build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase125 freestanding kernel invalidation build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase125BehaviorSlice(build_dir, build_targets);
    ExpectPhase125PublicationSlice(phase_doc_path,
                                   common_paths.roadmap_path,
                                   position_path,
                                   common_paths.kernel_readme_path,
                                   common_paths.repo_map_path,
                                   tool_readme_path,
                                   common_paths.freestanding_readme_path,
                                   common_paths.decision_log_path,
                                   common_paths.backlog_path);
    ExpectPhase125MirStructureSlice(dump_targets.mir, mir_projection_path);
}

}  // namespace mc::tool_tests