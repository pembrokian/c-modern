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

void ExpectPhase132BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase132_backpressure_run_output.txt",
                                                             "freestanding kernel phase132 backpressure and blocking run");
    ExpectExitCodeAtLeast(run_outcome,
                          132,
                          run_output,
                          "phase132 freestanding kernel backpressure and blocking run should preserve the landed phase132 slice");
}

void ExpectPhase132PublicationSlice(const std::filesystem::path& phase_doc_path,
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
                         "Phase 132 -- Backpressure And Blocking Audit",
                         "phase132 plan note should exist in canonical phase-doc style");
    ExpectOutputContains(phase_doc,
                         "Keep the compiler, sema, MIR, backend, ABI, target, and `hal` surfaces closed.",
                         "phase132 plan note should record the decision not to widen compiler work");
    ExpectOutputContains(phase_doc,
                         "Phase 132 is complete as one bounded backpressure and blocking audit.",
                         "phase132 plan note should close out the backpressure boundary explicitly");

    const std::string roadmap = ReadFile(roadmap_path);
    ExpectOutputContains(roadmap,
                         "Phase 132 is now concrete as one bounded backpressure and blocking audit",
                         "phase132 roadmap should record the landed backpressure boundary");

    const std::string position = ReadFile(position_path);
    ExpectOutputContains(position,
                         "landed Phase 132 backpressure and blocking audit",
                         "phase132 position note should record the landed backpressure step");

    const std::string kernel_readme = ReadFile(kernel_readme_path);
    ExpectOutputContains(kernel_readme,
                         "bounded backpressure and blocking audit",
                         "phase132 kernel README should describe the new IPC-pressure boundary");

    const std::string repo_map = ReadFile(repo_map_path);
    ExpectOutputContains(repo_map,
                         "phase132_backpressure_and_blocking.cpp",
                         "phase132 repository map should list the new kernel proof owner");

    const std::string tool_readme = ReadFile(tool_readme_path);
    ExpectOutputContains(tool_readme,
                         "kernel/phase132_backpressure_and_blocking.cpp",
                         "phase132 tool README should list the new kernel proof owner");

    const std::string freestanding_readme = ReadFile(freestanding_readme_path);
    ExpectOutputContains(freestanding_readme,
                         "phase132_backpressure_and_blocking.cpp",
                         "phase132 freestanding README should list the new kernel proof owner");

    const std::string decision_log = ReadFile(decision_log_path);
    ExpectOutputContains(decision_log,
                         "Phase 132 backpressure and blocking audit",
                         "phase132 decision log should record the limited-scope backpressure decision");

    const std::string backlog = ReadFile(backlog_path);
    ExpectOutputContains(backlog,
                         "keep the Phase 132 backpressure and blocking audit explicit",
                         "phase132 backlog should keep the new backpressure boundary visible for later phases");
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

void RunFreestandingKernelPhase132BackpressureAndBlocking(const std::filesystem::path& source_root,
                                                          const std::filesystem::path& binary_root,
                                                          const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path phase_doc_path = source_root / "docs" / "plan" / "active" /
                                                 "phase132_backpressure_and_blocking_audit.txt";
    const std::filesystem::path position_path = source_root / "docs" / "plan" / "admin" /
                                                "modern_c_canopus_readiness_position.txt";
    const std::filesystem::path tool_readme_path = source_root / "tests" / "tool" / "README.md";
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
    ExpectPhase132BehaviorSlice(build_dir, build_targets);
    ExpectPhase132PublicationSlice(phase_doc_path,
                                   ResolveCanopusRoadmapPath(source_root, 132),
                                   position_path,
                                   common_paths.kernel_readme_path,
                                   common_paths.repo_map_path,
                                   tool_readme_path,
                                   common_paths.freestanding_readme_path,
                                   common_paths.decision_log_path,
                                   common_paths.backlog_path);
    ExpectPhase132MirStructureSlice(dump_targets.mir, mir_projection_path);
}

}  // namespace mc::tool_tests