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

void RunFreestandingKernelPhase110KernelOwnershipSplitAudit(const std::filesystem::path& source_root,
                                                            const std::filesystem::path& binary_root,
                                                            const std::filesystem::path& mc_path) {
    const std::filesystem::path project_path = source_root / "kernel" / "build.toml";
    const std::filesystem::path main_source_path = source_root / "kernel" / "src/main.mc";
    const std::filesystem::path debug_source_path = source_root / "kernel" / "src/debug.mc";
    const std::filesystem::path sched_source_path = source_root / "kernel" / "src/sched.mc";
    const std::filesystem::path phase_doc_path = source_root / "docs" / "plan" /
                                                 "phase110_kernel_ownership_split_audit.txt";
    const std::filesystem::path kernel_readme_path = source_root / "kernel" / "README.md";
    const std::filesystem::path repo_map_path = source_root / "docs" / "agent" / "prompts" / "repo_map.md";
    const std::filesystem::path freestanding_readme_path = source_root / "tests" / "tool" / "freestanding" / "README.md";
    const std::filesystem::path build_dir = binary_root / "kernel_phase110_ownership_split_build";
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
                                                                 build_dir / "kernel_phase110_ownership_split_build_output.txt",
                                                                 "freestanding kernel phase110 ownership-split audit build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase110 freestanding kernel ownership-split audit build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(main_source_path, build_dir);
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase110_ownership_split_run_output.txt",
                                                             "freestanding kernel phase110 ownership-split audit run");
    if (!run_outcome.exited || run_outcome.exit_code != 110) {
        Fail("phase110 freestanding kernel ownership-split audit run should exit with the current kernel proof marker:\n" +
             run_output);
    }

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_debug.mc.o")) {
        Fail("phase110 ownership split should emit a distinct debug module object");
    }
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_sched.mc.o")) {
        Fail("phase110 ownership split should emit a distinct sched module object");
    }

    const std::string phase_doc = ReadFile(phase_doc_path);
    ExpectOutputContains(phase_doc,
                         "Phase 110 -- Kernel Ownership Split Audit",
                         "phase110 plan note should exist in canonical phase-doc style");
    ExpectOutputContains(phase_doc,
                         "scheduler-owned lifecycle validation is no longer defined in",
                         "phase110 plan note should state the concrete ownership split exit criterion");
    ExpectOutputContains(phase_doc,
                         "Phase 110 is complete as a kernel ownership split audit.",
                         "phase110 plan note should close out the ownership split explicitly");

    const std::string kernel_readme = ReadFile(kernel_readme_path);
    ExpectOutputContains(kernel_readme,
                         "Phase 110 has moved the repository-owned kernel artifact beyond the landed",
                         "phase110 kernel README should record the ownership split as current status");
    ExpectOutputContains(kernel_readme,
                         "src/sched.mc",
                         "phase110 kernel README should list the scheduler-owned module");
    ExpectOutputContains(kernel_readme,
                         "src/debug.mc",
                         "phase110 kernel README should list the debug-owned module");

    const std::string repo_map = ReadFile(repo_map_path);
    ExpectOutputContains(repo_map,
                         "currently a Phase 110 kernel-ownership-split-audited kernel target",
                         "phase110 repository map should describe the current kernel boundary");
    ExpectOutputContains(repo_map,
                         "src/debug.mc",
                         "phase110 repository map should list the debug owner");
    ExpectOutputContains(repo_map,
                         "src/sched.mc",
                         "phase110 repository map should list the scheduler owner");

    const std::string freestanding_readme = ReadFile(freestanding_readme_path);
    ExpectOutputContains(freestanding_readme,
                         "phase110_kernel_ownership_split_audit.cpp",
                         "phase110 freestanding README should list the new kernel proof owner");

    const std::string main_source = ReadFile(main_source_path);
    ExpectOutputContains(main_source,
                         "import debug",
                         "phase110 main module should import the debug owner explicitly");
    ExpectOutputContains(main_source,
                         "import sched",
                         "phase110 main module should import the scheduler owner explicitly");
    ExpectOutputContains(main_source,
                         "sched.validate_program_cap_spawn_and_wait",
                         "phase110 main module should call the scheduler-owned lifecycle validator");
    ExpectOutputContains(main_source,
                         "debug.validate_phase110_kernel_ownership_split",
                         "phase110 main module should call the debug-owned ownership split audit");

    const std::string debug_source = ReadFile(debug_source_path);
    ExpectOutputContains(debug_source,
                         "func validate_phase109_first_running_kernel_slice(audit: RunningKernelSliceAudit) bool",
                         "phase110 debug module should retain the landed phase109 running-slice audit");
    ExpectOutputContains(debug_source,
                         "func validate_phase110_kernel_ownership_split(audit: RunningKernelSliceAudit, scheduler_contract_hardened: u32) bool",
                         "phase110 debug module should own the new ownership split audit");

    const std::string sched_source = ReadFile(sched_source_path);
    ExpectOutputContains(sched_source,
                         "func validate_program_cap_spawn_and_wait(audit: LifecycleAudit) bool",
                         "phase110 sched module should own the lifecycle validation path");

    const std::string kernel_mir = ReadFile(dump_targets.mir);
    ExpectOutputContains(kernel_mir,
                         "ConstGlobal names=[PHASE110_MARKER] type=i32",
                         "phase110 merged MIR should expose the current kernel marker");
    ExpectOutputContains(kernel_mir,
                         "Function name=sched.validate_program_cap_spawn_and_wait returns=[bool]",
                         "phase110 merged MIR should expose the scheduler-owned lifecycle validator");
    ExpectOutputContains(kernel_mir,
                         "Function name=debug.validate_phase108_kernel_image_and_program_cap_contracts returns=[bool]",
                         "phase110 merged MIR should retain the landed phase108 debug audit path");
    ExpectOutputContains(kernel_mir,
                         "Function name=debug.validate_phase109_first_running_kernel_slice returns=[bool]",
                         "phase110 merged MIR should retain the landed phase109 running-slice audit");
    ExpectOutputContains(kernel_mir,
                         "Function name=debug.validate_phase110_kernel_ownership_split returns=[bool]",
                         "phase110 merged MIR should expose the new ownership split audit");
}

}  // namespace mc::tool_tests