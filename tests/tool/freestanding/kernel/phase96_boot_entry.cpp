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
using mc::test_support::WriteFile;
void RunFreestandingKernelPhase96BootEntryProof(const std::filesystem::path& source_root,
                                                const std::filesystem::path& binary_root,
                                                const std::filesystem::path& mc_path) {
    const std::filesystem::path project_path = source_root / "kernel" / "build.toml";
    const std::filesystem::path main_source_path = source_root / "kernel" / "src/main.mc";
    const std::filesystem::path build_dir = binary_root / "kernel_boot_entry_build";
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
                                                                 build_dir / "kernel_boot_entry_build_output.txt",
                                                                 "freestanding kernel boot entry build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase96 freestanding kernel boot entry build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(main_source_path, build_dir);
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_boot_entry_run_output.txt",
                                                             "freestanding kernel boot entry run");
    if (!run_outcome.exited || run_outcome.exit_code != 96) {
        Fail("phase96 freestanding kernel boot entry run should exit with the boot-entry proof marker:\n" +
             run_output);
    }

    const std::string kernel_mir = ReadFile(dump_targets.mir);
    ExpectOutputContains(kernel_mir,
                         "TypeDecl kind=struct name=state.KernelDescriptor",
                         "phase96 merged MIR should preserve the imported kernel descriptor type");
    ExpectOutputContains(kernel_mir,
                         "TypeDecl kind=struct name=endpoint.EndpointTable",
                         "phase96 merged MIR should preserve the imported endpoint-table type");
    ExpectOutputContains(kernel_mir,
                         "TypeDecl kind=struct name=interrupt.InterruptController",
                         "phase96 merged MIR should preserve the imported interrupt-controller type");
    ExpectOutputContains(kernel_mir,
                         "TypeDecl kind=struct name=state.ReadyQueue",
                         "phase96 merged MIR should preserve the imported ready-queue type");
    ExpectOutputContains(kernel_mir,
                         "VarGlobal names=[KERNEL] type=state.KernelDescriptor",
                         "phase96 merged MIR should retain the mutable kernel descriptor global");
    ExpectOutputContains(kernel_mir,
                         "VarGlobal names=[ROOT_CAPABILITY] type=capability.CapabilitySlot",
                         "phase96 merged MIR should retain the mutable root capability global");
    ExpectOutputContains(kernel_mir,
                         "VarGlobal names=[BOOT_LOG] type=state.BootLog",
                         "phase96 merged MIR should retain the mutable boot log global");
    ExpectOutputContains(kernel_mir,
                         "Function name=architecture_entry returns=[bool]",
                         "phase96 merged MIR should keep explicit architecture entry in the root proof module");
    ExpectOutputContains(kernel_mir,
                         "Function name=state.append_record returns=[state.BootLog]",
                         "phase96 merged MIR should keep boot-record shaping inside an ordinary helper function boundary");
    ExpectOutputContains(kernel_mir,
                         "store_target target=KERNEL target_kind=global target_name=KERNEL",
                         "phase96 merged MIR should lower kernel descriptor writes as global targets");
    ExpectOutputContains(kernel_mir,
                         "store_target target=ROOT_CAPABILITY target_kind=global target_name=ROOT_CAPABILITY",
                         "phase96 merged MIR should lower capability-root writes as global targets");
    ExpectOutputContains(kernel_mir,
                         "store_target target=BOOT_LOG target_kind=global target_name=BOOT_LOG",
                         "phase96 merged MIR should lower boot-log writes as global targets");
    ExpectOutputContains(kernel_mir,
                         "aggregate_init %v",
                         "phase96 merged MIR should use aggregate initialization for kernel-owned tables");
    ExpectOutputContains(kernel_mir,
                         "variant_match",
                         "phase96 merged MIR should lower kernel state classification through ordinary enum matching");
}

}  // namespace

namespace mc::tool_tests {

}  // namespace mc::tool_tests
