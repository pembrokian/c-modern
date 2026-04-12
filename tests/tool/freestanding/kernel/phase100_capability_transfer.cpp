#include <filesystem>
#include <string>

#include "compiler/support/dump_paths.h"
#include "compiler/support/target.h"
#include "tests/support/process_utils.h"
#include "tests/tool/tool_suite_common.h"

namespace mc::tool_tests {

using mc::test_support::ExpectOutputContains;
using mc::test_support::Fail;
using mc::test_support::ReadFile;
using mc::test_support::RunCommandCapture;

void RunFreestandingKernelPhase100CapabilityTransfer(const std::filesystem::path& source_root,
                                                     const std::filesystem::path& binary_root,
                                                     const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path build_dir = binary_root / "kernel_capability_transfer_build";
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
                                                                 build_dir / "kernel_capability_transfer_build_output.txt",
                                                                 "freestanding kernel capability-transfer build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase100 freestanding kernel capability-transfer build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_capability_transfer_run_output.txt",
                                                             "freestanding kernel capability-transfer run");
    if (!run_outcome.exited || run_outcome.exit_code != 100) {
        Fail("phase100 freestanding kernel capability-transfer run should exit with the capability-transfer proof marker:\n" +
             run_output);
    }

    ExpectTextContainsLinesFile(ReadFile(dump_targets.mir),
                                ResolveFreestandingKernelGoldenPath(source_root,
                                                                    "phase100_capability_transfer.mir.contains.txt"),
                                "phase100 merged MIR should preserve the capability-transfer proof slice");
}

}  // namespace mc::tool_tests