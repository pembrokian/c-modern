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

void RunFreestandingKernelPhase113InterruptEntryAndGenericDispatchBoundary(const std::filesystem::path& source_root,
                                                                           const std::filesystem::path& binary_root,
                                                                           const std::filesystem::path& mc_path) {
    const std::filesystem::path project_path = source_root / "kernel" / "build.toml";
    const std::filesystem::path main_source_path = source_root / "kernel" / "src" / "main.mc";
    const std::filesystem::path interrupt_source_path = source_root / "kernel" / "src" / "interrupt.mc";
    const std::filesystem::path debug_source_path = source_root / "kernel" / "src" / "debug.mc";
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
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase113_interrupt_boundary_run_output.txt",
                                                             "freestanding kernel phase113 interrupt boundary run");
    if (!run_outcome.exited || run_outcome.exit_code != 113) {
        Fail("phase113 freestanding kernel interrupt boundary run should exit with the current kernel proof marker:\n" +
             run_output);
    }

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_interrupt.mc.o")) {
        Fail("phase113 interrupt boundary audit should emit the interrupt module object");
    }

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
                         "after Phase 113 closed the first",
                         "phase113 position note should advance the current repository position");
    ExpectOutputContains(position,
                         "interrupt-entry and generic-dispatch boundary.",
                         "phase113 position note should reference the new closeout");

    const std::string kernel_readme = ReadFile(kernel_readme_path);
    ExpectOutputContains(kernel_readme,
                         "Phase 113 has moved the repository-owned kernel artifact beyond the landed",
                         "phase113 kernel README should record the interrupt boundary as current status");
    ExpectOutputContains(kernel_readme,
                         "interrupt-entry and generic-dispatch boundary",
                         "phase113 kernel README should describe the interrupt owner");

    const std::string repo_map = ReadFile(repo_map_path);
    ExpectOutputContains(repo_map,
                         "currently a Phase 113 interrupt-entry-and-generic-dispatch-bounded kernel target",
                         "phase113 repository map should describe the current kernel boundary");
    ExpectOutputContains(repo_map,
                         "phase113_interrupt_entry_and_generic_dispatch_boundary.cpp",
                         "phase113 repository map should list the new kernel proof owner");

    const std::string freestanding_readme = ReadFile(freestanding_readme_path);
    ExpectOutputContains(freestanding_readme,
                         "phase113_interrupt_entry_and_generic_dispatch_boundary.cpp",
                         "phase113 freestanding README should list the new kernel proof owner");

    const std::string decision_log = ReadFile(decision_log_path);
    ExpectOutputContains(decision_log,
                         "Phase 113 interrupt entry and generic dispatch boundary",
                         "phase113 decision log should record the limited-scope kernel-only decision");

    const std::string backlog = ReadFile(backlog_path);
    ExpectOutputContains(backlog,
                         "keep the Phase 113 interrupt entry and generic dispatch boundary explicit",
                         "phase113 backlog should keep the new boundary visible for later phases");

    const std::string main_source = ReadFile(main_source_path);
    ExpectOutputContains(main_source,
                         "const PHASE113_MARKER: i32 = 113",
                         "phase113 main module should advance the current kernel marker");
    ExpectOutputContains(main_source,
                         "interrupt.arch_enter_interrupt",
                         "phase113 main module should route timer wake through interrupt entry");
    ExpectOutputContains(main_source,
                         "interrupt.dispatch_interrupt",
                         "phase113 main module should route timer wake through generic interrupt dispatch");
    ExpectOutputContains(main_source,
                         "interrupt.validate_interrupt_entry_and_dispatch_boundary",
                         "phase113 main module should validate the interrupt-owned boundary");
    ExpectOutputContains(main_source,
                         "debug.validate_phase113_interrupt_entry_and_generic_dispatch_boundary",
                         "phase113 main module should call the new debug-owned phase113 audit");

    const std::string interrupt_source = ReadFile(interrupt_source_path);
    ExpectOutputContains(interrupt_source,
                         "func arch_enter_interrupt(controller: InterruptController, vector: u32, source_actor: u32) InterruptEntry",
                         "phase113 interrupt module should own architecture-local entry");
    ExpectOutputContains(interrupt_source,
                         "func dispatch_interrupt(entry: InterruptEntry, timer_state: timer.TimerState, now_tick: u64) InterruptDispatchResult",
                         "phase113 interrupt module should own generic dispatch");
    ExpectOutputContains(interrupt_source,
                         "func validate_interrupt_entry_and_dispatch_boundary() bool",
                         "phase113 interrupt module should expose a focused owner-local validator");

    const std::string debug_source = ReadFile(debug_source_path);
    ExpectOutputContains(debug_source,
                         "func validate_phase113_interrupt_entry_and_generic_dispatch_boundary(audit: RunningKernelSliceAudit, scheduler_contract_hardened: u32, lifecycle_contract_hardened: u32, capability_contract_hardened: u32, ipc_contract_hardened: u32, address_space_contract_hardened: u32, interrupt_contract_hardened: u32, interrupt_dispatch_kind: interrupt.InterruptDispatchKind) bool",
                         "phase113 debug module should own the new interrupt-boundary audit");

    const std::string kernel_mir = ReadFile(dump_targets.mir);
    ExpectOutputContains(kernel_mir,
                         "ConstGlobal names=[PHASE113_MARKER] type=i32",
                         "phase113 merged MIR should expose the current kernel marker");
    ExpectOutputContains(kernel_mir,
                         "Function name=interrupt.validate_interrupt_entry_and_dispatch_boundary returns=[bool]",
                         "phase113 merged MIR should expose the interrupt-owned validator");
    ExpectOutputContains(kernel_mir,
                         "Function name=debug.validate_phase113_interrupt_entry_and_generic_dispatch_boundary returns=[bool]",
                         "phase113 merged MIR should expose the new debug-owned phase113 audit");
}

}  // namespace mc::tool_tests