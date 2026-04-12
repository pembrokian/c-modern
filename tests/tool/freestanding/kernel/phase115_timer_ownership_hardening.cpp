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

void ExpectPhase115BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase115_timer_ownership_run_output.txt",
                                                             "freestanding kernel phase115 timer-ownership run");
    ExpectExitCodeAtLeast(run_outcome,
                          115,
                          run_output,
                          "phase115 freestanding kernel timer-ownership run should preserve the landed phase115 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_timer.mc.o")) {
        Fail("phase115 timer-ownership audit should emit the timer module object");
    }
}

void ExpectPhase115PublicationSlice(const std::filesystem::path& phase_doc_path,
                                    const std::filesystem::path& roadmap_path,
                                    const std::filesystem::path& kernel_readme_path,
                                    const std::filesystem::path& repo_map_path,
                                    const std::filesystem::path& freestanding_readme_path,
                                    const std::filesystem::path& decision_log_path,
                                    const std::filesystem::path& backlog_path) {
    const std::string phase_doc = ReadFile(phase_doc_path);
    ExpectOutputContains(phase_doc,
                         "Phase 115 -- Timer Ownership Hardening",
                         "phase115 plan note should exist in canonical phase-doc style");
    ExpectOutputContains(phase_doc,
                         "Do not expand the compiler or language surface unless this kernel step",
                         "phase115 plan note should record the decision not to widen compiler work");
    ExpectOutputContains(phase_doc,
                         "Phase 115 is complete as a timer ownership hardening step.",
                         "phase115 plan note should close out the timer boundary explicitly");

    const std::string roadmap = ReadFile(roadmap_path);
    ExpectOutputContains(roadmap,
                         "Phase 115 is now concrete as one timer ownership hardening step",
                         "phase115 roadmap should record the landed timer boundary");

    const std::string kernel_readme = ReadFile(kernel_readme_path);
    ExpectOutputContains(kernel_readme,
                         "timer ownership hardening",
                         "phase115 kernel README should describe the timer owner");

    const std::string repo_map = ReadFile(repo_map_path);
    ExpectOutputContains(repo_map,
                         "phase115_timer_ownership_hardening.cpp",
                         "phase115 repository map should list the new kernel proof owner");

    const std::string freestanding_readme = ReadFile(freestanding_readme_path);
    ExpectOutputContains(freestanding_readme,
                         "phase115_timer_ownership_hardening.cpp",
                         "phase115 freestanding README should list the new kernel proof owner");

    const std::string decision_log = ReadFile(decision_log_path);
    ExpectOutputContains(decision_log,
                         "Phase 115 timer ownership hardening",
                         "phase115 decision log should record the limited-scope kernel-only decision");

    const std::string backlog = ReadFile(backlog_path);
    ExpectOutputContains(backlog,
                         "keep the Phase 115 timer ownership hardening explicit",
                         "phase115 backlog should keep the new boundary visible for later phases");
}

void ExpectPhase115MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "TypeDecl kind=struct name=timer.TimerInterruptDelivery",
            "Function name=timer.deliver_interrupt_tick returns=[timer.TimerInterruptDelivery]",
            "Function name=timer.validate_interrupt_delivery_boundary returns=[bool]",
            "Function name=debug.validate_phase115_timer_ownership_hardening returns=[bool]",
        },
        expected_projection_path,
        "phase115 merged MIR should preserve the timer-ownership projection");
}

}  // namespace

void RunFreestandingKernelPhase115TimerOwnershipHardening(const std::filesystem::path& source_root,
                                                          const std::filesystem::path& binary_root,
                                                          const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path phase_doc_path = ResolvePlanDocPath(source_root,
                                                                    "phase115_timer_ownership_hardening.txt");
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase115_timer_ownership_hardening.mirproj.txt";
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
                                                                 build_dir / "kernel_phase115_timer_ownership_build_output.txt",
                                                                 "freestanding kernel phase115 timer-ownership build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase115 freestanding kernel timer-ownership build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase115BehaviorSlice(build_dir, build_targets);
    ExpectPhase115PublicationSlice(phase_doc_path,
                                   ResolveCanopusRoadmapPath(source_root, 115),
                                   common_paths.kernel_readme_path,
                                   common_paths.repo_map_path,
                                   common_paths.freestanding_readme_path,
                                   common_paths.decision_log_path,
                                   common_paths.backlog_path);
    ExpectPhase115MirStructureSlice(dump_targets.mir, mir_projection_path);
}

}  // namespace mc::tool_tests