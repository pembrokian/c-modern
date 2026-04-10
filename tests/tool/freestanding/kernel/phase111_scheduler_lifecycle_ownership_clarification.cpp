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

void RunFreestandingKernelPhase111SchedulerLifecycleOwnershipClarification(const std::filesystem::path& source_root,
                                                                           const std::filesystem::path& binary_root,
                                                                           const std::filesystem::path& mc_path) {
    const std::filesystem::path project_path = source_root / "kernel" / "build.toml";
    const std::filesystem::path main_source_path = source_root / "kernel" / "src/main.mc";
    const std::filesystem::path debug_source_path = source_root / "kernel" / "src/debug.mc";
    const std::filesystem::path lifecycle_source_path = source_root / "kernel" / "src/lifecycle.mc";
    const std::filesystem::path syscall_source_path = source_root / "kernel" / "src/syscall.mc";
    const std::filesystem::path phase_doc_path = source_root / "docs" / "plan" /
                                                 "phase111_scheduler_lifecycle_ownership_clarification.txt";
    const std::filesystem::path kernel_readme_path = source_root / "kernel" / "README.md";
    const std::filesystem::path repo_map_path = source_root / "docs" / "agent" / "prompts" / "repo_map.md";
    const std::filesystem::path freestanding_readme_path = source_root / "tests" / "tool" / "freestanding" / "README.md";
    const std::filesystem::path build_dir = binary_root / "kernel_phase111_lifecycle_ownership_build";
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
                                                                 build_dir / "kernel_phase111_lifecycle_ownership_build_output.txt",
                                                                 "freestanding kernel phase111 lifecycle-ownership audit build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase111 freestanding kernel lifecycle-ownership audit build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(main_source_path, build_dir);
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase111_lifecycle_ownership_run_output.txt",
                                                             "freestanding kernel phase111 lifecycle-ownership audit run");
    if (!run_outcome.exited || run_outcome.exit_code != 111) {
        Fail("phase111 freestanding kernel lifecycle-ownership audit run should exit with the current kernel proof marker:\n" +
             run_output);
    }

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_lifecycle.mc.o")) {
        Fail("phase111 lifecycle clarification should emit a distinct lifecycle module object");
    }

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
                         "Phase 111 has moved the repository-owned kernel artifact beyond the landed",
                         "phase111 kernel README should record the lifecycle clarification as current status");
    ExpectOutputContains(kernel_readme,
                         "src/lifecycle.mc",
                         "phase111 kernel README should list the lifecycle-owned module");

    const std::string repo_map = ReadFile(repo_map_path);
    ExpectOutputContains(repo_map,
                         "currently a Phase 111 scheduler-and-lifecycle-ownership-clarified kernel target",
                         "phase111 repository map should describe the current kernel boundary");
    ExpectOutputContains(repo_map,
                         "src/lifecycle.mc",
                         "phase111 repository map should list the lifecycle owner");

    const std::string freestanding_readme = ReadFile(freestanding_readme_path);
    ExpectOutputContains(freestanding_readme,
                         "phase111_scheduler_lifecycle_ownership_clarification.cpp",
                         "phase111 freestanding README should list the new kernel proof owner");

    const std::string main_source = ReadFile(main_source_path);
    ExpectOutputContains(main_source,
                         "import lifecycle",
                         "phase111 main module should import the lifecycle owner explicitly");
    ExpectOutputContains(main_source,
                         "lifecycle.validate_task_transition_contracts",
                         "phase111 main module should validate the lifecycle-owned transition contract");
    ExpectOutputContains(main_source,
                         "lifecycle.ready_task",
                         "phase111 main module should route wake-to-ready through the lifecycle owner");
    ExpectOutputContains(main_source,
                         "debug.validate_phase111_scheduler_and_lifecycle_ownership",
                         "phase111 main module should call the debug-owned phase111 audit");

    const std::string syscall_source = ReadFile(syscall_source_path);
    ExpectOutputContains(syscall_source,
                         "import lifecycle",
                         "phase111 syscall module should import the lifecycle owner explicitly");
    ExpectOutputContains(syscall_source,
                         "lifecycle.install_spawned_child",
                         "phase111 syscall module should route spawn installation through the lifecycle owner");
    ExpectOutputContains(syscall_source,
                         "lifecycle.block_task_on_timer",
                         "phase111 syscall module should route timer blocking through the lifecycle owner");
    ExpectOutputContains(syscall_source,
                         "lifecycle.release_waited_child_slots",
                         "phase111 syscall module should route waited-child release through the lifecycle owner");

    const std::string lifecycle_source = ReadFile(lifecycle_source_path);
    ExpectOutputContains(lifecycle_source,
                         "func install_spawned_child(",
                         "phase111 lifecycle module should own spawned-child slot installation");
    ExpectOutputContains(lifecycle_source,
                         "func block_task_on_timer(",
                         "phase111 lifecycle module should own timer-block transition");
    ExpectOutputContains(lifecycle_source,
                         "func ready_task(",
                         "phase111 lifecycle module should own wake-to-ready transition");
    ExpectOutputContains(lifecycle_source,
                         "func validate_task_transition_contracts() bool",
                         "phase111 lifecycle module should own a focused transition contract audit");

    const std::string debug_source = ReadFile(debug_source_path);
    ExpectOutputContains(debug_source,
                         "func validate_phase111_scheduler_and_lifecycle_ownership(audit: RunningKernelSliceAudit, scheduler_contract_hardened: u32, lifecycle_contract_hardened: u32) bool",
                         "phase111 debug module should own the new lifecycle clarification audit");

    const std::string kernel_mir = ReadFile(dump_targets.mir);
    ExpectOutputContains(kernel_mir,
                         "ConstGlobal names=[PHASE111_MARKER] type=i32",
                         "phase111 merged MIR should expose the current kernel marker");
    ExpectOutputContains(kernel_mir,
                         "Function name=lifecycle.install_spawned_child",
                         "phase111 merged MIR should expose the lifecycle-owned spawn transition");
    ExpectOutputContains(kernel_mir,
                         "Function name=lifecycle.block_task_on_timer",
                         "phase111 merged MIR should expose the lifecycle-owned block transition");
    ExpectOutputContains(kernel_mir,
                         "Function name=lifecycle.ready_task",
                         "phase111 merged MIR should expose the lifecycle-owned ready transition");
    ExpectOutputContains(kernel_mir,
                         "Function name=debug.validate_phase111_scheduler_and_lifecycle_ownership returns=[bool]",
                         "phase111 merged MIR should expose the new phase111 debug audit");
}

}  // namespace mc::tool_tests