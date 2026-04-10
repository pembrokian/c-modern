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

void RunFreestandingKernelPhase109FirstRunningKernelSliceAudit(const std::filesystem::path& source_root,
                                                               const std::filesystem::path& binary_root,
                                                               const std::filesystem::path& mc_path) {
    const std::filesystem::path project_path = source_root / "kernel" / "build.toml";
    const std::filesystem::path main_source_path = source_root / "kernel" / "src/main.mc";
    const std::filesystem::path phase_doc_path = source_root / "docs" / "plan" /
                                                 "phase109_first_running_canopus_kernel_slice_audit.txt";
    const std::filesystem::path kernel_readme_path = source_root / "kernel" / "README.md";
    const std::filesystem::path repo_map_path = source_root / "docs" / "agent" / "prompts" / "repo_map.md";
    const std::filesystem::path build_dir = binary_root / "kernel_phase109_running_slice_build";
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
                                                                 build_dir / "kernel_phase109_running_slice_build_output.txt",
                                                                 "freestanding kernel phase109 running-slice audit build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase109 freestanding kernel running-slice audit build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(main_source_path, build_dir);
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase109_running_slice_run_output.txt",
                                                             "freestanding kernel phase109 running-slice audit run");
    if (!run_outcome.exited || run_outcome.exit_code != 112) {
        Fail("phase109 freestanding kernel running-slice audit run should exit with the current kernel proof marker:\n" +
             run_output);
    }

    const std::string phase_doc = ReadFile(phase_doc_path);
    ExpectOutputContains(phase_doc,
                         "Phase 109 -- First Running Canopus Kernel Slice Audit",
                         "phase109 plan note should exist in canonical phase-doc style");
    ExpectOutputContains(phase_doc,
                         "explicit boot entry, first user entry, endpoint-and-handle object",
                         "phase109 plan note should publish the admitted running-kernel slice explicitly");
    ExpectOutputContains(phase_doc,
                         "Phase 109 is complete as a first running Canopus kernel slice audit.",
                         "phase109 plan note should close out the running-kernel support statement explicitly");

    const std::string kernel_readme = ReadFile(kernel_readme_path);
    ExpectOutputContains(kernel_readme,
                         "Phase 112 has moved the repository-owned kernel artifact beyond the landed",
                         "phase109 kernel README should record the current kernel status while preserving the phase109 slice");
    ExpectOutputContains(kernel_readme,
                         "Phase 112 syscall-boundary thinness audit",
                         "phase109 kernel README should preserve the landed running-slice support statement");

    const std::string repo_map = ReadFile(repo_map_path);
    ExpectOutputContains(repo_map,
                         "currently a Phase 112 syscall-boundary-thinness-audited kernel target",
                         "phase109 repository map should describe the current kernel boundary while retaining the phase109 slice");
    ExpectOutputContains(repo_map,
                         "Phase 109 first-running-kernel-slice audit",
                         "phase109 repository map should still list the landed phase109 kernel proof owner");

    const std::string kernel_mir = ReadFile(dump_targets.mir);
    ExpectOutputContains(kernel_mir,
                         "ConstGlobal names=[PHASE112_MARKER] type=i32",
                         "phase109 merged MIR should expose the current running-kernel marker");
    ExpectOutputContains(kernel_mir,
                         "Function name=debug.validate_phase108_kernel_image_and_program_cap_contracts returns=[bool]",
                         "phase109 merged MIR should preserve the landed phase108 image/program-cap audit path");
    ExpectOutputContains(kernel_mir,
                         "Function name=debug.validate_phase109_first_running_kernel_slice returns=[bool]",
                         "phase109 merged MIR should retain the aggregate first-running-kernel audit path");
}

}  // namespace mc::tool_tests