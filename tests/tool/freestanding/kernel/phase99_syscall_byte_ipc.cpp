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

void RunFreestandingKernelPhase99SyscallByteIpc(const std::filesystem::path& source_root,
                                                const std::filesystem::path& binary_root,
                                                const std::filesystem::path& mc_path) {
    const std::filesystem::path project_path = source_root / "kernel" / "build.toml";
    const std::filesystem::path main_source_path = source_root / "kernel" / "src/main.mc";
    const std::filesystem::path build_dir = binary_root / "kernel_syscall_byte_ipc_build";
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
                                                                 build_dir / "kernel_syscall_byte_ipc_build_output.txt",
                                                                 "freestanding kernel syscall-byte-ipc build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase99 freestanding kernel syscall-byte-ipc build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(main_source_path, build_dir);
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_syscall_byte_ipc_run_output.txt",
                                                             "freestanding kernel syscall-byte-ipc run");
    if (!run_outcome.exited || run_outcome.exit_code != 99) {
        Fail("phase99 freestanding kernel syscall-byte-ipc run should exit with the syscall-byte-ipc proof marker:\n" +
             run_output);
    }

    const std::string kernel_mir = ReadFile(dump_targets.mir);
    ExpectOutputContains(kernel_mir,
                         "TypeDecl kind=struct name=syscall.SendRequest",
                         "phase99 merged MIR should preserve the imported send-request type");
    ExpectOutputContains(kernel_mir,
                         "TypeDecl kind=struct name=syscall.ReceiveObservation",
                         "phase99 merged MIR should preserve the imported receive-observation type");
    ExpectOutputContains(kernel_mir,
                         "TypeDecl kind=struct name=syscall.SendResult",
                         "phase99 merged MIR should preserve the imported send-result type");
    ExpectOutputContains(kernel_mir,
                         "VarGlobal names=[RECEIVE_OBSERVATION] type=syscall.ReceiveObservation",
                         "phase99 merged MIR should retain the syscall receive-observation global");
    ExpectOutputContains(kernel_mir,
                         "VarGlobal names=[SYSCALL_GATE] type=syscall.SyscallGate",
                         "phase99 merged MIR should retain the widened syscall-gate global");
    ExpectOutputContains(kernel_mir,
                         "Function name=execute_syscall_byte_ipc returns=[bool]",
                         "phase99 merged MIR should keep explicit syscall-byte-ipc execution in the root proof module");
    ExpectOutputContains(kernel_mir,
                         "Function name=syscall.perform_send returns=[syscall.SendResult]",
                         "phase99 merged MIR should keep byte-only send inside the syscall helper boundary");
    ExpectOutputContains(kernel_mir,
                         "Function name=syscall.perform_receive returns=[syscall.ReceiveResult]",
                         "phase99 merged MIR should keep byte-only receive inside the syscall helper boundary");
    ExpectOutputContains(kernel_mir,
                         "store_target target=SYSCALL_GATE target_kind=global target_name=SYSCALL_GATE",
                         "phase99 merged MIR should lower syscall-gate writes as global targets");
    ExpectOutputContains(kernel_mir,
                         "store_target target=RECEIVE_OBSERVATION target_kind=global target_name=RECEIVE_OBSERVATION",
                         "phase99 merged MIR should lower receive-observation writes as global targets");
    ExpectOutputContains(kernel_mir,
                         "store_target target=ENDPOINTS target_kind=global target_name=ENDPOINTS",
                         "phase99 merged MIR should lower endpoint-table writes as global targets during syscall send and receive");
    ExpectOutputContains(kernel_mir,
                         "aggregate_init %v",
                         "phase99 merged MIR should use aggregate initialization for syscall requests and observations");
    ExpectOutputContains(kernel_mir,
                         "variant_match",
                         "phase99 merged MIR should lower syscall status classification through ordinary enum matching");
}

}  // namespace mc::tool_tests