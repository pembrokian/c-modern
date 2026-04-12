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

void RunFreestandingKernelPhase107RealUserToUserCapabilityTransfer(const std::filesystem::path& source_root,
                                                                   const std::filesystem::path& binary_root,
                                                                   const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
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
                                                                 build_dir / "kernel_phase107_user_transfer_build_output.txt",
                                                                 "freestanding kernel phase107 user-to-user capability transfer build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase107 freestanding kernel user-to-user capability transfer build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase107_user_transfer_run_output.txt",
                                                             "freestanding kernel phase107 user-to-user capability transfer run");
    ExpectExitCodeAtLeast(run_outcome,
                          107,
                          run_output,
                          "phase107 freestanding kernel user-to-user capability transfer run should preserve the landed phase107 slice");

    const std::string kernel_mir = ReadFile(dump_targets.mir);
    ExpectOutputContains(kernel_mir,
                         "Function name=validate_phase106_echo_service_request_reply returns=[bool]",
                         "phase107 merged MIR should preserve the landed phase106 echo-service proof");
    ExpectOutputContains(kernel_mir,
                         "TypeDecl kind=struct name=transfer_service.TransferServiceState",
                         "phase107 merged MIR should declare the bounded transfer-service state record");
    ExpectOutputContains(kernel_mir,
                         "TypeDecl kind=struct name=transfer_service.TransferObservation",
                         "phase107 merged MIR should declare the bounded transfer observation record");
    ExpectOutputContains(kernel_mir,
                         "Function name=execute_phase107_user_to_user_capability_transfer returns=[bool]",
                         "phase107 merged MIR should keep the real user-to-user proof in the root kernel module");
    ExpectOutputContains(kernel_mir,
                         "Function name=transfer_service.record_grant returns=[transfer_service.TransferServiceState]",
                         "phase107 merged MIR should keep grant observation inside the transfer-service helper boundary");
    ExpectOutputContains(kernel_mir,
                         "Function name=transfer_service.emit_payload returns=[[4]u8]",
                         "phase107 merged MIR should keep emit-payload construction inside the transfer-service helper boundary");
    ExpectOutputContains(kernel_mir,
                         "Function name=transfer_service.observe_transfer returns=[transfer_service.TransferObservation]",
                         "phase107 merged MIR should keep the final transfer observation inside the transfer-service helper boundary");
    ExpectOutputContains(kernel_mir,
                         "Function name=validate_phase107_user_to_user_capability_transfer returns=[bool]",
                         "phase107 merged MIR should retain the end-to-end validation path for the real user-to-user slice");
}

}  // namespace mc::tool_tests