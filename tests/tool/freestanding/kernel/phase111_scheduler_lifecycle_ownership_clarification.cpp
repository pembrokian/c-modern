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

void ExpectPhase111BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase111_lifecycle_ownership_run_output.txt",
                                                             "freestanding kernel phase111 lifecycle-ownership audit run");
    ExpectExitCodeAtLeast(run_outcome,
                          111,
                          run_output,
                          "phase111 freestanding kernel lifecycle-ownership audit run should preserve the landed phase111 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_lifecycle.mc.o")) {
        Fail("phase111 lifecycle clarification should emit a distinct lifecycle module object");
    }
}

void ExpectPhase111PublicationSlice(const std::filesystem::path& phase_doc_path,
                                    const std::filesystem::path& kernel_readme_path,
                                    const std::filesystem::path& repo_map_path,
                                    const std::filesystem::path& freestanding_readme_path) {
    const std::string phase_doc = ReadFile(phase_doc_path);
    ExpectOutputContains(phase_doc,
                         "Phase 111 -- Scheduler And Lifecycle Ownership Clarification",
                         "phase111 plan note should exist in canonical phase-doc style");
    ExpectOutputContains(phase_doc,
                         "kernel/src/lifecycle.mc",
                         "phase111 plan note should name the concrete lifecycle owner");
    ExpectOutputContains(phase_doc,
                         "Phase 111 is complete as a scheduler and lifecycle ownership clarification.",
                         "phase111 plan note should close out the ownership clarification explicitly");

    const std::string kernel_readme = ReadFile(kernel_readme_path);
    ExpectOutputContains(kernel_readme,
                         "src/lifecycle.mc",
                         "phase111 kernel README should list the lifecycle-owned module");

    const std::string repo_map = ReadFile(repo_map_path);
    ExpectOutputContains(repo_map,
                         "src/lifecycle.mc",
                         "phase111 repository map should list the lifecycle owner");

    const std::string freestanding_readme = ReadFile(freestanding_readme_path);
    ExpectOutputContains(freestanding_readme,
                         "phase111_scheduler_lifecycle_ownership_clarification.cpp",
                         "phase111 freestanding README should list the new kernel proof owner");
}

void ExpectPhase111MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "Function name=lifecycle.install_spawned_child returns=[lifecycle.SpawnInstallResult]",
            "Function name=lifecycle.block_task_on_timer returns=[lifecycle.TaskTransition]",
            "Function name=lifecycle.ready_task returns=[lifecycle.TaskTransition]",
            "Function name=lifecycle.validate_task_transition_contracts returns=[bool]",
            "Function name=debug.validate_phase111_scheduler_and_lifecycle_ownership returns=[bool]",
        },
        expected_projection_path,
        "phase111 merged MIR should preserve the lifecycle ownership projection");
}

}  // namespace

void RunFreestandingKernelPhase111SchedulerLifecycleOwnershipClarification(const std::filesystem::path& source_root,
                                                                           const std::filesystem::path& binary_root,
                                                                           const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path phase_doc_path = ResolvePlanDocPath(source_root,
                                                                    "phase111_scheduler_lifecycle_ownership_clarification.txt");
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase111_scheduler_lifecycle_ownership_clarification.mirproj.txt";
    const std::filesystem::path build_dir = binary_root / "kernel_phase111_lifecycle_ownership_build";
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
                                                                 build_dir / "kernel_phase111_lifecycle_ownership_build_output.txt",
                                                                 "freestanding kernel phase111 lifecycle-ownership audit build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase111 freestanding kernel lifecycle-ownership audit build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase111BehaviorSlice(build_dir, build_targets);
    ExpectPhase111PublicationSlice(phase_doc_path,
                                   common_paths.kernel_readme_path,
                                   common_paths.repo_map_path,
                                   common_paths.freestanding_readme_path);
    ExpectPhase111MirStructureSlice(dump_targets.mir, mir_projection_path);
}

}  // namespace mc::tool_tests