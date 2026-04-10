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

void ExpectPhase110BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase110_ownership_split_run_output.txt",
                                                             "freestanding kernel phase110 ownership-split audit run");
    if (!run_outcome.exited || run_outcome.exit_code != 116) {
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
}

void ExpectPhase110PublicationSlice(const std::filesystem::path& phase_doc_path,
                                    const std::filesystem::path& kernel_readme_path,
                                    const std::filesystem::path& repo_map_path,
                                    const std::filesystem::path& freestanding_readme_path) {
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
                         "Phase 116 has moved the repository-owned kernel artifact beyond the landed",
                         "phase110 kernel README should record the ownership split as current status");
    ExpectOutputContains(kernel_readme,
                         "src/sched.mc",
                         "phase110 kernel README should list the scheduler-owned module");
    ExpectOutputContains(kernel_readme,
                         "src/debug.mc",
                         "phase110 kernel README should list the debug-owned module");

    const std::string repo_map = ReadFile(repo_map_path);
    ExpectOutputContains(repo_map,
                         "currently a Phase 116 MMU-activation-barrier-hardened kernel target",
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
}

void ExpectPhase110MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "ConstGlobal names=[PHASE116_MARKER] type=i32",
            "Function name=debug.validate_phase108_kernel_image_and_program_cap_contracts returns=[bool]",
            "Function name=debug.validate_phase109_first_running_kernel_slice returns=[bool]",
            "Function name=debug.validate_phase110_kernel_ownership_split returns=[bool]",
            "Function name=sched.validate_program_cap_spawn_and_wait returns=[bool]",
        },
        expected_projection_path,
        "phase110 merged MIR should preserve the ownership split projection");
}

}  // namespace

void RunFreestandingKernelPhase110KernelOwnershipSplitAudit(const std::filesystem::path& source_root,
                                                            const std::filesystem::path& binary_root,
                                                            const std::filesystem::path& mc_path) {
    const std::filesystem::path project_path = source_root / "kernel" / "build.toml";
    const std::filesystem::path main_source_path = source_root / "kernel" / "src/main.mc";
    const std::filesystem::path phase_doc_path = source_root / "docs" / "plan" /
                                                 "phase110_kernel_ownership_split_audit.txt";
    const std::filesystem::path kernel_readme_path = source_root / "kernel" / "README.md";
    const std::filesystem::path repo_map_path = source_root / "docs" / "agent" / "prompts" / "repo_map.md";
    const std::filesystem::path freestanding_readme_path = source_root / "tests" / "tool" / "freestanding" / "README.md";
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase110_kernel_ownership_split_audit.mirproj.txt";
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
    ExpectPhase110BehaviorSlice(build_dir, build_targets);
    ExpectPhase110PublicationSlice(phase_doc_path, kernel_readme_path, repo_map_path, freestanding_readme_path);
    ExpectPhase110MirStructureSlice(dump_targets.mir, mir_projection_path);
}

}  // namespace mc::tool_tests