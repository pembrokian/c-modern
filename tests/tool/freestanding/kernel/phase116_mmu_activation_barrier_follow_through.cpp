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

void ExpectPhase116BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase116_mmu_barrier_run_output.txt",
                                                             "freestanding kernel phase116 mmu barrier run");
    if (!run_outcome.exited || run_outcome.exit_code != 116) {
        Fail("phase116 freestanding kernel mmu barrier run should exit with the current kernel proof marker:\n" +
             run_output);
    }

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_mmu.mc.o")) {
        Fail("phase116 mmu barrier audit should emit the mmu module object");
    }
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_stdlib_hal.mc.o")) {
        Fail("phase116 mmu barrier audit should emit the hal module object");
    }
}

void ExpectPhase116PublicationSlice(const std::filesystem::path& phase_doc_path,
                                    const std::filesystem::path& roadmap_path,
                                    const std::filesystem::path& position_path,
                                    const std::filesystem::path& kernel_readme_path,
                                    const std::filesystem::path& repo_map_path,
                                    const std::filesystem::path& freestanding_readme_path,
                                    const std::filesystem::path& freestanding_support_path,
                                    const std::filesystem::path& stdlib_readme_path,
                                    const std::filesystem::path& decision_log_path,
                                    const std::filesystem::path& backlog_path) {
    const std::string phase_doc = ReadFile(phase_doc_path);
    ExpectOutputContains(phase_doc,
                         "Phase 116 -- MMU Activation Barrier Follow-Through",
                         "phase116 plan note should exist in canonical phase-doc style");
    ExpectOutputContains(phase_doc,
                         "Do not widen the compiler beyond one narrow `hal.memory_barrier()` hook",
                         "phase116 plan note should record the narrow compiler-boundary decision");
    ExpectOutputContains(phase_doc,
                         "Phase 116 is complete as an MMU activation barrier follow-through.",
                         "phase116 plan note should close out the MMU barrier boundary explicitly");

    const std::string roadmap = ReadFile(roadmap_path);
    ExpectOutputContains(roadmap,
                         "Phase 116 is now concrete as one MMU activation barrier follow-through",
                         "phase116 roadmap should record the landed barrier-facing hook");

    const std::string position = ReadFile(position_path);
    ExpectOutputContains(position,
                         "after Phase 116 closed the MMU activation barrier follow-through",
                         "phase116 position note should advance the current repository position");
    ExpectOutputContains(position,
                         "landed Phase 116 MMU activation barrier follow-through.",
                         "phase116 position note should reference the new closeout");

    const std::string kernel_readme = ReadFile(kernel_readme_path);
    ExpectOutputContains(kernel_readme,
                         "Phase 116 has moved the repository-owned kernel artifact beyond the landed",
                         "phase116 kernel README should record the new hardware-boundary status");
    ExpectOutputContains(kernel_readme,
                         "MMU activation barrier follow-through",
                         "phase116 kernel README should describe the barrier-facing MMU owner");

    const std::string repo_map = ReadFile(repo_map_path);
    ExpectOutputContains(repo_map,
                         "currently a Phase 116 MMU-activation-barrier-hardened kernel target",
                         "phase116 repository map should describe the current kernel boundary");
    ExpectOutputContains(repo_map,
                         "phase116_mmu_activation_barrier_follow_through.cpp",
                         "phase116 repository map should list the new kernel proof owner");

    const std::string freestanding_readme = ReadFile(freestanding_readme_path);
    ExpectOutputContains(freestanding_readme,
                         "phase116_mmu_activation_barrier_follow_through.cpp",
                         "phase116 freestanding README should list the new kernel proof owner");

    const std::string freestanding_support = ReadFile(freestanding_support_path);
    ExpectOutputContains(freestanding_support,
                         "memory_barrier()",
                         "phase116 freestanding support note should expose the widened hal surface");

    const std::string stdlib_readme = ReadFile(stdlib_readme_path);
    ExpectOutputContains(stdlib_readme,
                         "hal.memory_barrier()",
                         "phase116 stdlib README should record the widened hal surface");

    const std::string decision_log = ReadFile(decision_log_path);
    ExpectOutputContains(decision_log,
                         "Phase 116 MMU activation barrier follow-through",
                         "phase116 decision log should record the limited-scope barrier decision");

    const std::string backlog = ReadFile(backlog_path);
    ExpectOutputContains(backlog,
                         "keep the Phase 116 MMU activation barrier follow-through explicit",
                         "phase116 backlog should keep the new barrier boundary visible for later phases");
}

void ExpectPhase116MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "ConstGlobal names=[PHASE116_MARKER] type=i32",
            "TypeDecl kind=struct name=mmu.ActivationBarrierObservation",
            "Function name=hal.memory_barrier",
            "Function name=mmu.activate_with_barrier returns=[mmu.ActivationBarrierObservation]",
            "Function name=mmu.validate_activation_barrier_boundary returns=[bool]",
            "Function name=debug.validate_phase116_mmu_activation_barrier_follow_through returns=[bool]",
        },
        expected_projection_path,
        "phase116 merged MIR should preserve the MMU barrier projection");
}

}  // namespace

void RunFreestandingKernelPhase116MmuActivationBarrierFollowThrough(const std::filesystem::path& source_root,
                                                                    const std::filesystem::path& binary_root,
                                                                    const std::filesystem::path& mc_path) {
    const std::filesystem::path project_path = source_root / "kernel" / "build.toml";
    const std::filesystem::path main_source_path = source_root / "kernel" / "src" / "main.mc";
    const std::filesystem::path phase_doc_path = source_root / "docs" / "plan" /
                                                 "phase116_mmu_activation_barrier_follow_through.txt";
    const std::filesystem::path roadmap_path = source_root / "docs" / "plan" / "admin" /
                                               "canopus_post_phase109_speculative_roadmap.txt";
    const std::filesystem::path position_path = source_root / "docs" / "plan" / "admin" /
                                                "modern_c_canopus_readiness_position.txt";
    const std::filesystem::path kernel_readme_path = source_root / "kernel" / "README.md";
    const std::filesystem::path repo_map_path = source_root / "docs" / "agent" / "prompts" / "repo_map.md";
    const std::filesystem::path freestanding_readme_path = source_root / "tests" / "tool" / "freestanding" / "README.md";
    const std::filesystem::path freestanding_support_path = source_root / "docs" / "plan" /
                                                            "freestanding_support_statement.txt";
    const std::filesystem::path stdlib_readme_path = source_root / "stdlib" / "README.md";
    const std::filesystem::path decision_log_path = source_root / "docs" / "plan" / "decision_log.txt";
    const std::filesystem::path backlog_path = source_root / "docs" / "plan" / "backlog.txt";
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase116_mmu_activation_barrier_follow_through.mirproj.txt";
    const std::filesystem::path build_dir = binary_root / "kernel_phase116_mmu_barrier_build";
    std::filesystem::remove_all(build_dir);

    const auto [build_outcome, build_output] = RunCommandCapture({mc_path.generic_string(),
                                                                  "build",
                                                                  "--project",
                                                                  project_path.generic_string(),
                                                                  "--target",
                                                                  "kernel",
                                                                  "--build-dir",
                                                                  build_dir.generic_string(),
                                                                  "--dump-mir"},
                                                                 build_dir / "kernel_phase116_mmu_barrier_build_output.txt",
                                                                 "freestanding kernel phase116 mmu barrier build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase116 freestanding kernel mmu barrier build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(main_source_path, build_dir);
    ExpectPhase116BehaviorSlice(build_dir, build_targets);
    ExpectPhase116PublicationSlice(phase_doc_path,
                                   roadmap_path,
                                   position_path,
                                   kernel_readme_path,
                                   repo_map_path,
                                   freestanding_readme_path,
                                   freestanding_support_path,
                                   stdlib_readme_path,
                                   decision_log_path,
                                   backlog_path);
    ExpectPhase116MirStructureSlice(dump_targets.mir, mir_projection_path);
}

}  // namespace mc::tool_tests