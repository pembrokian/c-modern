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

    const std::string kernel_mir = ReadFile(dump_targets.mir);
    ExpectOutputContains(kernel_mir,
                         "TypeDecl kind=struct name=syscall.SendRequest",
                         "phase100 merged MIR should preserve the widened send-request type");
    ExpectOutputContains(kernel_mir,
                         "TypeDecl kind=struct name=syscall.ReceiveObservation",
                         "phase100 merged MIR should preserve the widened receive-observation type");
    ExpectOutputContains(kernel_mir,
                         "VarGlobal names=[ATTACHED_RECEIVE_OBSERVATION] type=syscall.ReceiveObservation",
                         "phase100 merged MIR should retain the attached-handle receive observation global");
    ExpectOutputContains(kernel_mir,
                         "VarGlobal names=[TRANSFERRED_HANDLE_USE_OBSERVATION] type=syscall.ReceiveObservation",
                         "phase100 merged MIR should retain the transferred-handle follow-through observation global");
    ExpectOutputContains(kernel_mir,
                         "Function name=execute_capability_carrying_ipc_transfer returns=[bool]",
                         "phase100 merged MIR should keep explicit capability-carrying IPC execution in the root proof module");
    ExpectOutputContains(kernel_mir,
                         "Function name=seed_transfer_endpoint_handle returns=[bool]",
                         "phase100 merged MIR should keep explicit transferable-endpoint seeding in the root proof module");
    ExpectOutputContains(kernel_mir,
                         "Function name=capability.remove_handle returns=[capability.HandleTable]",
                         "phase100 merged MIR should keep sender-side handle removal inside the capability helper boundary");
    ExpectOutputContains(kernel_mir,
                         "Function name=syscall.build_transfer_send_request returns=[syscall.SendRequest]",
                         "phase100 merged MIR should keep attached-handle send construction inside the syscall helper boundary");
    ExpectOutputContains(kernel_mir,
                         "Function name=syscall.build_transfer_receive_request returns=[syscall.ReceiveRequest]",
                         "phase100 merged MIR should keep attached-handle receive construction inside the syscall helper boundary");
    ExpectOutputContains(kernel_mir,
                         "store_target target=HANDLE_TABLES target_kind=global target_name=HANDLE_TABLES",
                         "phase100 merged MIR should lower handle-table updates as global targets during transfer send and receive");
    ExpectOutputContains(kernel_mir,
                         "store_target target=ATTACHED_RECEIVE_OBSERVATION target_kind=global target_name=ATTACHED_RECEIVE_OBSERVATION",
                         "phase100 merged MIR should lower attached-handle observations as global targets");
    ExpectOutputContains(kernel_mir,
                         "aggregate_init %v",
                         "phase100 merged MIR should use aggregate initialization for transfer requests and observations");
    ExpectOutputContains(kernel_mir,
                         "variant_match",
                         "phase100 merged MIR should lower attached-handle status classification through ordinary enum matching");
}

}  // namespace mc::tool_tests