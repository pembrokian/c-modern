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

void ExpectPhase123BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase123_next_plateau_run_output.txt",
                                                             "freestanding kernel phase123 next-plateau audit run");
    ExpectExitCodeAtLeast(run_outcome,
                          123,
                          run_output,
                          "phase123 freestanding kernel next-plateau run should preserve the landed phase123 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_bootstrap_audit.mc.o")) {
        Fail("phase123 next-plateau audit should emit the bootstrap_audit module object");
    }
}

void ExpectPhase123PublicationSlice(const std::filesystem::path& phase_doc_path,
                                    const std::filesystem::path& roadmap_path,
                                    const std::filesystem::path& position_path,
                                    const std::filesystem::path& kernel_readme_path,
                                    const std::filesystem::path& repo_map_path,
                                    const std::filesystem::path& freestanding_readme_path,
                                    const std::filesystem::path& decision_log_path,
                                    const std::filesystem::path& backlog_path) {
    const std::string phase_doc = ReadFile(phase_doc_path);
    ExpectOutputContains(phase_doc,
                         "Phase 123 -- Next Plateau Audit",
                         "phase123 plan note should exist in canonical phase-doc style");
    ExpectOutputContains(phase_doc,
                         "Keep the compiler, sema, MIR, backend, ABI, and `hal` surfaces closed.",
                         "phase123 plan note should record the decision not to widen compiler work");
    ExpectOutputContains(phase_doc,
                         "Phase 123 is complete as a next-plateau audit.",
                         "phase123 plan note should close out the plateau boundary explicitly");

    const std::string roadmap = ReadFile(roadmap_path);
    ExpectOutputContains(roadmap,
                         "Phase 123 is now concrete as one next-plateau audit",
                         "phase123 roadmap should record the landed plateau boundary");

    const std::string position = ReadFile(position_path);
    ExpectOutputContains(position,
                         "landed Phase 123 next-plateau audit",
                         "phase123 position note should record the landed plateau step");

    const std::string kernel_readme = ReadFile(kernel_readme_path);
    ExpectOutputContains(kernel_readme,
                         "bounded next-plateau audit",
                         "phase123 kernel README should describe the new plateau boundary");

    const std::string repo_map = ReadFile(repo_map_path);
    ExpectOutputContains(repo_map,
                         "phase123_next_plateau_audit.cpp",
                         "phase123 repository map should list the new kernel proof owner");

    const std::string freestanding_readme = ReadFile(freestanding_readme_path);
    ExpectOutputContains(freestanding_readme,
                         "phase123_next_plateau_audit.cpp",
                         "phase123 freestanding README should list the new kernel proof owner");

    const std::string decision_log = ReadFile(decision_log_path);
    ExpectOutputContains(decision_log,
                         "Phase 123 next-plateau audit",
                         "phase123 decision log should record the limited-scope plateau decision");

    const std::string backlog = ReadFile(backlog_path);
    ExpectOutputContains(backlog,
                         "keep the Phase 123 next-plateau audit explicit",
                         "phase123 backlog should keep the plateau boundary visible for later phases");
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

}  // namespace

void RunFreestandingKernelPhase123NextPlateauAudit(const std::filesystem::path& source_root,
                                                   const std::filesystem::path& binary_root,
                                                   const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path phase_doc_path = source_root / "docs" / "plan" /
                                                 "phase123_next_plateau_audit.txt";
    const std::filesystem::path position_path = source_root / "docs" / "plan" / "admin" /
                                                "modern_c_canopus_readiness_position.txt";
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase123_next_plateau_audit.mirproj.txt";
    const std::filesystem::path build_dir = binary_root / "kernel_phase123_next_plateau_build";
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
                                                                 build_dir / "kernel_phase123_next_plateau_build_output.txt",
                                                                 "freestanding kernel phase123 next-plateau audit build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase123 freestanding kernel next-plateau audit build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase123BehaviorSlice(build_dir, build_targets);
    ExpectPhase123PublicationSlice(phase_doc_path,
                                   common_paths.roadmap_path,
                                   position_path,
                                   common_paths.kernel_readme_path,
                                   common_paths.repo_map_path,
                                   common_paths.freestanding_readme_path,
                                   common_paths.decision_log_path,
                                   common_paths.backlog_path);
    ExpectPhase123MirStructureSlice(dump_targets.mir, mir_projection_path);
}

}  // namespace mc::tool_tests