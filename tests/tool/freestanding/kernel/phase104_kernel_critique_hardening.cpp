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

void RunFreestandingKernelPhase104CritiqueHardening(const std::filesystem::path& source_root,
                                                    const std::filesystem::path& binary_root,
                                                    const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path build_dir = binary_root / "kernel_phase104_hardening_build";
    std::filesystem::remove_all(build_dir);

    const auto [build_outcome, build_output] = RunCommandCapture({mc_path.generic_string(),
                                                                  "build",
                                                                  "--project",
                                                                  common_paths.project_path.generic_string(),
                                                                  "--target",
                                                                  "kernel",
                                                                  "--build-dir",
                                                                  build_dir.generic_string(),
                                                                  "--dump-mir"},
                                                                 build_dir / "kernel_phase104_hardening_build_output.txt",
                                                                 "freestanding kernel phase104 hardening build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase104 freestanding kernel hardening build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase104_hardening_run_output.txt",
                                                             "freestanding kernel phase104 hardening run");
    if (!run_outcome.exited || run_outcome.exit_code != 104) {
        Fail("phase104 freestanding kernel hardening run should exit with the hardening proof marker:\n" +
             run_output);
    }

    const std::string kernel_mir = ReadFile(dump_targets.mir);
    ExpectOutputContains(kernel_mir,
                         "TypeDecl kind=struct name=state.BootLogAppendResult",
                         "phase104 merged MIR should preserve the explicit boot-log append result type");
    ExpectOutputContains(kernel_mir,
                         "VarGlobal names=[BOOT_LOG_APPEND_FAILED] type=u32",
                         "phase104 merged MIR should retain the visible boot-log append failure flag");
    ExpectOutputContains(kernel_mir,
                         "Function name=validate_phase104_contract_hardening returns=[bool]",
                         "phase104 merged MIR should keep the critique hardening proof in the root kernel module");
    ExpectOutputContains(kernel_mir,
                         "Function name=validate_timer_hardening_contracts returns=[bool]",
                         "phase104 merged MIR should keep timer hardening checks in the root proof module");
    ExpectOutputContains(kernel_mir,
                         "Function name=validate_bootstrap_layout_contracts returns=[bool]",
                         "phase104 merged MIR should keep bootstrap layout validation in the root proof module");
    ExpectOutputContains(kernel_mir,
                         "Function name=validate_endpoint_and_capability_contracts returns=[bool]",
                         "phase104 merged MIR should keep endpoint and capability contract checks in the root proof module");
    ExpectOutputContains(kernel_mir,
                         "Function name=validate_state_hardening_contracts returns=[bool]",
                         "phase104 merged MIR should keep state hardening checks in the root proof module");
    ExpectOutputContains(kernel_mir,
                         "Function name=timer.has_fired_sleeper returns=[u32]",
                         "phase104 merged MIR should keep explicit timer drain discovery inside the timer helper boundary");
    ExpectOutputContains(kernel_mir,
                         "Function name=address_space.bootstrap_layout_valid returns=[bool]",
                         "phase104 merged MIR should keep bootstrap layout validation inside the address-space helper boundary");
    ExpectOutputContains(kernel_mir,
                         "Function name=capability.handle_install_succeeded returns=[bool]",
                         "phase104 merged MIR should keep handle-install success detection inside the capability helper boundary");
    ExpectOutputContains(kernel_mir,
                         "Function name=endpoint.enqueue_succeeded returns=[bool]",
                         "phase104 merged MIR should keep enqueue success detection inside the endpoint helper boundary");
    ExpectOutputContains(kernel_mir,
                         "Function name=state.append_record returns=[state.BootLogAppendResult]",
                         "phase104 merged MIR should keep boot-log overflow visibility inside the state helper boundary");
    ExpectOutputContains(kernel_mir,
                         "TypeDecl kind=enum name=syscall.BlockReason",
                         "phase104 merged MIR should keep explicit syscall block reasons instead of collapsing all blocking states into a bare status");
    ExpectOutputContains(kernel_mir,
                         "Function name=syscall.resolve_endpoint returns=[syscall.EndpointResolution]",
                         "phase104 merged MIR should keep endpoint resolution inside a dedicated syscall helper");
    ExpectOutputContains(kernel_mir,
                         "Function name=syscall.admit_spawn returns=[syscall.SpawnAdmission]",
                         "phase104 merged MIR should keep spawn admission separate from spawn commit logic");
    ExpectOutputContains(kernel_mir,
                         "Function name=syscall.release_waited_child returns=[syscall.WaitRelease]",
                         "phase104 merged MIR should keep waited-child cleanup in a dedicated helper instead of hardcoded slot branches");
    ExpectOutputContains(kernel_mir,
                         "Function name=capability.handle_remove_succeeded returns=[bool]",
                         "phase104 merged MIR should keep explicit handle-removal success detection inside the capability helper boundary");
    ExpectOutputContains(kernel_mir,
                         "Function name=state.find_empty_process_slot returns=[u32]",
                         "phase104 merged MIR should keep process-slot selection inside the state helper boundary");
    ExpectOutputContains(kernel_mir,
                         "Function name=capability.attenuate_endpoint_rights returns=[u32]",
                         "phase104 merged MIR should keep endpoint-rights transfer attenuation explicit inside the capability helper boundary");
    ExpectOutputContains(kernel_mir,
                         "Function name=execute_child_sleep_transition returns=[bool]",
                         "phase104 merged MIR should keep the child sleep proof stage split into a dedicated root-proof helper");
    ExpectOutputContains(kernel_mir,
                         "Function name=execute_child_post_exit_wait returns=[bool]",
                         "phase104 merged MIR should keep the post-exit wait proof stage split into a dedicated root-proof helper");
    ExpectOutputContains(kernel_mir,
                         "aggregate_init %v",
                         "phase104 merged MIR should continue using aggregate initialization for the hardened helper records");
    ExpectOutputContains(kernel_mir,
                         "variant_match",
                         "phase104 merged MIR should continue lowering the hardened state checks through ordinary enum matching");
}

}  // namespace mc::tool_tests