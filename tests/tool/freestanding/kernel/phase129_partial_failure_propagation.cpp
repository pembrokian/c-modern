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

void ExpectPhase129BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase129_partial_failure_run_output.txt",
                                                             "freestanding kernel phase129 partial failure run");
    ExpectExitCodeAtLeast(run_outcome,
                          129,
                          run_output,
                          "phase129 freestanding kernel partial-failure run should preserve the landed phase129 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "kernel__bootstrap_audit.o")) {
        Fail("phase129 partial-failure audit should emit the bootstrap_audit module object");
    }
}

void ExpectPhase129PublicationSlice(const std::filesystem::path& phase_doc_path,
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
                         "Phase 129 -- Partial Failure Propagation",
                         "phase129 plan note should exist in canonical phase-doc style");
    ExpectOutputContains(phase_doc,
                         "Keep the compiler, sema, MIR, backend, ABI, and `hal` surfaces closed.",
                         "phase129 plan note should record the decision not to widen compiler work");
    ExpectOutputContains(phase_doc,
                         "Phase 129 is complete as a partial failure propagation step.",
                         "phase129 plan note should close out the partial-failure boundary explicitly");

    const std::string roadmap = ReadFile(roadmap_path);
    ExpectOutputContains(roadmap,
                         "Phase 129 is now concrete as one partial failure propagation step",
                         "phase129 roadmap should record the landed partial-failure boundary");

    const std::string position = ReadFile(position_path);
    ExpectOutputContains(position,
                         "landed Phase 129 partial failure propagation step",
                         "phase129 position note should record the landed partial-failure step");

    const std::string kernel_readme = ReadFile(kernel_readme_path);
    ExpectOutputContains(kernel_readme,
                         "bounded partial failure propagation step",
                         "phase129 kernel README should describe the new failure boundary");

    const std::string repo_map = ReadFile(repo_map_path);
    ExpectOutputContains(repo_map,
                         "phase129_partial_failure_propagation.cpp",
                         "phase129 repository map should list the new kernel proof owner");

    const std::string tool_readme = ReadFile(tool_readme_path);
    ExpectOutputContains(tool_readme,
                         "kernel/phase129_partial_failure_propagation.cpp",
                         "phase129 tool README should list the new kernel proof owner");

    const std::string freestanding_readme = ReadFile(freestanding_readme_path);
    ExpectOutputContains(freestanding_readme,
                         "phase129_partial_failure_propagation.cpp",
                         "phase129 freestanding README should list the new kernel proof owner");

    const std::string decision_log = ReadFile(decision_log_path);
    ExpectOutputContains(decision_log,
                         "Phase 129 partial failure propagation",
                         "phase129 decision log should record the limited-scope failure decision");

    const std::string backlog = ReadFile(backlog_path);
    ExpectOutputContains(backlog,
                         "keep the Phase 129 partial failure propagation explicit",
                         "phase129 backlog should keep the new failure boundary visible for later phases");
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

}  // namespace

void RunFreestandingKernelPhase129PartialFailurePropagation(const std::filesystem::path& source_root,
                                                            const std::filesystem::path& binary_root,
                                                            const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path phase_doc_path = source_root / "docs" / "plan" /
                                                 "phase129_partial_failure_propagation.txt";
    const std::filesystem::path position_path = source_root / "docs" / "plan" / "admin" /
                                                "modern_c_canopus_readiness_position.txt";
    const std::filesystem::path tool_readme_path = source_root / "tests" / "tool" / "README.md";
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase129_partial_failure_propagation.mirproj.txt";
    const std::filesystem::path build_dir = binary_root / "kernel_phase129_partial_failure_build";
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
                                                                 build_dir / "kernel_phase129_partial_failure_build_output.txt",
                                                                 "freestanding kernel phase129 partial failure build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase129 freestanding kernel partial-failure build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase129BehaviorSlice(build_dir, build_targets);
    ExpectPhase129PublicationSlice(phase_doc_path,
                                   common_paths.roadmap_path,
                                   position_path,
                                   common_paths.kernel_readme_path,
                                   common_paths.repo_map_path,
                                   tool_readme_path,
                                   common_paths.freestanding_readme_path,
                                   common_paths.decision_log_path,
                                   common_paths.backlog_path);
    ExpectPhase129MirStructureSlice(dump_targets.mir, mir_projection_path);
}

}  // namespace mc::tool_tests