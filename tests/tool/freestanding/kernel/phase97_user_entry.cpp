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

void RunFreestandingKernelPhase97UserEntryProof(const std::filesystem::path& source_root,
                                                const std::filesystem::path& binary_root,
                                                const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path build_dir = binary_root / "kernel_user_entry_build";
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
                                                                 build_dir / "kernel_user_entry_build_output.txt",
                                                                 "freestanding kernel user-entry build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase97 freestanding kernel user-entry build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_user_entry_run_output.txt",
                                                             "freestanding kernel user-entry run");
    if (!run_outcome.exited || run_outcome.exit_code != 97) {
        Fail("phase97 freestanding kernel user-entry run should exit with the user-entry proof marker:\n" +
             run_output);
    }

    const std::string kernel_mir = ReadFile(dump_targets.mir);
    ExpectOutputContains(kernel_mir,
                         "TypeDecl kind=struct name=address_space.AddressSpace",
                         "phase97 merged MIR should preserve the imported address-space type");
    ExpectOutputContains(kernel_mir,
                         "TypeDecl kind=struct name=address_space.UserEntryFrame",
                         "phase97 merged MIR should preserve the imported user-entry frame type");
    ExpectOutputContains(kernel_mir,
                         "VarGlobal names=[ADDRESS_SPACE] type=address_space.AddressSpace",
                         "phase97 merged MIR should retain the mutable address-space global");
    ExpectOutputContains(kernel_mir,
                         "VarGlobal names=[USER_FRAME] type=address_space.UserEntryFrame",
                         "phase97 merged MIR should retain the mutable user-entry frame global");
    ExpectOutputContains(kernel_mir,
                         "VarGlobal names=[INIT_PROGRAM_CAPABILITY] type=capability.CapabilitySlot",
                         "phase97 merged MIR should retain the mutable init-program capability global");
    ExpectOutputContains(kernel_mir,
                         "Function name=construct_first_user_address_space",
                         "phase97 merged MIR should keep explicit first-user address-space construction in the root proof module");
    ExpectOutputContains(kernel_mir,
                         "Function name=transfer_to_first_user_entry returns=[bool]",
                         "phase97 merged MIR should keep explicit first-user transfer in the root proof module");
    ExpectOutputContains(kernel_mir,
                         "Function name=address_space.bootstrap_space returns=[address_space.AddressSpace]",
                         "phase97 merged MIR should keep address-space shaping inside an ordinary helper boundary");
    ExpectOutputContains(kernel_mir,
                         "store_target target=ADDRESS_SPACE target_kind=global target_name=ADDRESS_SPACE",
                         "phase97 merged MIR should lower address-space writes as global targets");
    ExpectOutputContains(kernel_mir,
                         "store_target target=USER_FRAME target_kind=global target_name=USER_FRAME",
                         "phase97 merged MIR should lower user-entry frame writes as global targets");
    ExpectOutputContains(kernel_mir,
                         "store_target target=INIT_PROGRAM_CAPABILITY target_kind=global target_name=INIT_PROGRAM_CAPABILITY",
                         "phase97 merged MIR should lower init-program capability writes as global targets");
    ExpectOutputContains(kernel_mir,
                         "aggregate_init %v",
                         "phase97 merged MIR should use aggregate initialization for the address-space and entry-state records");
    ExpectOutputContains(kernel_mir,
                         "variant_match",
                         "phase97 merged MIR should lower address-space and state classification through ordinary enum matching");
}

}  // namespace mc::tool_tests