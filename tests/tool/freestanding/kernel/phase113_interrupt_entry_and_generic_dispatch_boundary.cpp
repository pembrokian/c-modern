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

void ExpectPhase113BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase113_interrupt_boundary_run_output.txt",
                                                             "freestanding kernel phase113 interrupt boundary run");
    if (!run_outcome.exited || run_outcome.exit_code != 116) {
        Fail("phase113 freestanding kernel interrupt boundary run should exit with the current kernel proof marker:\n" +
             run_output);
    }

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_interrupt.mc.o")) {
        Fail("phase113 interrupt boundary audit should emit the interrupt module object");
    }
}

void ExpectPhase113PublicationSlice(const std::filesystem::path& phase_doc_path,
                                    const std::filesystem::path& roadmap_path,
                                    const std::filesystem::path& position_path,
                                    const std::filesystem::path& kernel_readme_path,
                                    const std::filesystem::path& repo_map_path,
                                    const std::filesystem::path& freestanding_readme_path,
                                    const std::filesystem::path& decision_log_path,
                                    const std::filesystem::path& backlog_path) {
    const std::string phase_doc = ReadFile(phase_doc_path);
    ExpectOutputContains(phase_doc,
                         "Phase 113 -- Interrupt Entry And Generic Dispatch Boundary",
                         "phase113 plan note should exist in canonical phase-doc style");
    ExpectOutputContains(phase_doc,
                         "Do not expand the compiler or language surface unless this kernel step",
                         "phase113 plan note should record the decision not to widen compiler work");
    ExpectOutputContains(phase_doc,
                         "Phase 113 is complete as an interrupt entry and generic dispatch boundary.",
                         "phase113 plan note should close out the interrupt boundary explicitly");

    const std::string roadmap = ReadFile(roadmap_path);
    ExpectOutputContains(roadmap,
                         "Phase 113 is now concrete as one interrupt-entry and generic-dispatch boundary",
                         "phase113 roadmap should record the landed interrupt boundary");

    const std::string position = ReadFile(position_path);
    ExpectOutputContains(position,
                         "after Phase 116 closed the MMU activation barrier follow-through",
                         "phase113 position note should advance the current repository position");
    ExpectOutputContains(position,
                         "MMU activation barrier follow-through.",
                         "phase113 position note should reference the new closeout");

    const std::string kernel_readme = ReadFile(kernel_readme_path);
    ExpectOutputContains(kernel_readme,
                         "Phase 116 has moved the repository-owned kernel artifact beyond the landed",
                         "phase113 kernel README should record the interrupt boundary as current status");
    ExpectOutputContains(kernel_readme,
                         "MMU activation barrier follow-through",
                         "phase113 kernel README should describe the interrupt owner");

    const std::string repo_map = ReadFile(repo_map_path);
    ExpectOutputContains(repo_map,
                         "currently a Phase 116 MMU-activation-barrier-hardened kernel target",
                         "phase113 repository map should describe the current kernel boundary");
    ExpectOutputContains(repo_map,
                         "phase113_interrupt_entry_and_generic_dispatch_boundary.cpp",
                         "phase113 repository map should list the new kernel proof owner");

    const std::string freestanding_readme = ReadFile(freestanding_readme_path);
    ExpectOutputContains(freestanding_readme,
                         "phase113_interrupt_entry_and_generic_dispatch_boundary.cpp",
                         "phase113 freestanding README should list the new kernel proof owner");
    ExpectOutputContains(freestanding_readme,
                         "phase116_mmu_activation_barrier_follow_through.cpp",
                         "phase113 freestanding README should reflect the next landed kernel proof owner");

    const std::string decision_log = ReadFile(decision_log_path);
    ExpectOutputContains(decision_log,
                         "Phase 113 interrupt entry and generic dispatch boundary",
                         "phase113 decision log should record the limited-scope kernel-only decision");

    const std::string backlog = ReadFile(backlog_path);
    ExpectOutputContains(backlog,
                         "keep the Phase 113 interrupt entry and generic dispatch boundary explicit",
                         "phase113 backlog should keep the new boundary visible for later phases");
}

void ExpectPhase113MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "ConstGlobal names=[PHASE116_MARKER] type=i32",
            "Function name=interrupt.arch_enter_interrupt returns=[interrupt.InterruptEntry]",
            "Function name=interrupt.dispatch_interrupt returns=[interrupt.InterruptDispatchResult]",
            "Function name=interrupt.validate_interrupt_entry_and_dispatch_boundary returns=[bool]",
            "Function name=debug.validate_phase113_interrupt_entry_and_generic_dispatch_boundary returns=[bool]",
        },
        expected_projection_path,
        "phase113 merged MIR should preserve the interrupt boundary projection");
}

}  // namespace

void RunFreestandingKernelPhase113InterruptEntryAndGenericDispatchBoundary(const std::filesystem::path& source_root,
                                                                           const std::filesystem::path& binary_root,
                                                                           const std::filesystem::path& mc_path) {
    const std::filesystem::path project_path = source_root / "kernel" / "build.toml";
    const std::filesystem::path main_source_path = source_root / "kernel" / "src" / "main.mc";
    const std::filesystem::path phase_doc_path = source_root / "docs" / "plan" /
                                                 "phase113_interrupt_entry_and_generic_dispatch_boundary.txt";
    const std::filesystem::path roadmap_path = source_root / "docs" / "plan" / "admin" /
                                               "canopus_post_phase109_speculative_roadmap.txt";
    const std::filesystem::path position_path = source_root / "docs" / "plan" / "admin" /
                                                "modern_c_canopus_readiness_position.txt";
    const std::filesystem::path kernel_readme_path = source_root / "kernel" / "README.md";
    const std::filesystem::path repo_map_path = source_root / "docs" / "agent" / "prompts" / "repo_map.md";
    const std::filesystem::path freestanding_readme_path = source_root / "tests" / "tool" / "freestanding" / "README.md";
    const std::filesystem::path decision_log_path = source_root / "docs" / "plan" / "decision_log.txt";
    const std::filesystem::path backlog_path = source_root / "docs" / "plan" / "backlog.txt";
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase113_interrupt_entry_and_generic_dispatch_boundary.mirproj.txt";
    const std::filesystem::path build_dir = binary_root / "kernel_phase113_interrupt_boundary_build";
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
                                                                 build_dir / "kernel_phase113_interrupt_boundary_build_output.txt",
                                                                 "freestanding kernel phase113 interrupt boundary build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase113 freestanding kernel interrupt boundary build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(main_source_path, build_dir);
    ExpectPhase113BehaviorSlice(build_dir, build_targets);
    ExpectPhase113PublicationSlice(phase_doc_path,
                                   roadmap_path,
                                   position_path,
                                   kernel_readme_path,
                                   repo_map_path,
                                   freestanding_readme_path,
                                   decision_log_path,
                                   backlog_path);
    ExpectPhase113MirStructureSlice(dump_targets.mir, mir_projection_path);
}


}  // namespace mc::tool_tests