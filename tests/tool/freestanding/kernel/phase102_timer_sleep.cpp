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

void RunFreestandingKernelPhase102TimerSleep(const std::filesystem::path& source_root,
                                             const std::filesystem::path& binary_root,
                                             const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path build_dir = binary_root / "kernel_timer_sleep_build";
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
                                                                 build_dir / "kernel_timer_sleep_build_output.txt",
                                                                 "freestanding kernel timer-sleep build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase102 freestanding kernel timer-sleep build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_timer_sleep_run_output.txt",
                                                             "freestanding kernel timer-sleep run");
    if (!run_outcome.exited || run_outcome.exit_code != 102) {
        Fail("phase102 freestanding kernel timer-sleep run should exit with the timer-sleep proof marker:\n" +
             run_output);
    }

    const std::string kernel_mir = ReadFile(dump_targets.mir);
    ExpectOutputContains(kernel_mir,
                         "TypeDecl kind=struct name=syscall.SleepRequest",
                         "phase102 merged MIR should preserve the imported sleep-request type");
    ExpectOutputContains(kernel_mir,
                         "TypeDecl kind=struct name=syscall.SleepObservation",
                         "phase102 merged MIR should preserve the imported sleep-observation type");
    ExpectOutputContains(kernel_mir,
                         "VarGlobal names=[SLEEP_OBSERVATION] type=syscall.SleepObservation",
                         "phase102 merged MIR should retain the sleep observation global");
    ExpectOutputContains(kernel_mir,
                         "VarGlobal names=[TIMER_WAKE_OBSERVATION] type=timer.TimerWakeObservation",
                         "phase102 merged MIR should retain the timer wake observation global");
    ExpectOutputContains(kernel_mir,
                         "VarGlobal names=[TIMER_STATE] type=timer.TimerState",
                         "phase102 merged MIR should retain the timer state global");
    ExpectOutputContains(kernel_mir,
                         "Function name=execute_program_cap_spawn_and_wait returns=[bool]",
                         "phase102 merged MIR should keep timer-backed child execution in the root proof module");
    ExpectOutputContains(kernel_mir,
                         "Function name=validate_program_cap_spawn_and_wait returns=[bool]",
                         "phase102 merged MIR should keep timer-backed validation in the root proof module");
    ExpectOutputContains(kernel_mir,
                         "Function name=syscall.perform_sleep returns=[syscall.SleepResult]",
                         "phase102 merged MIR should keep sleep observation inside the syscall helper boundary");
    ExpectOutputContains(kernel_mir,
                         "Function name=timer.arm_sleep returns=[timer.TimerState]",
                         "phase102 merged MIR should keep timer arming inside the timer helper boundary");
    ExpectOutputContains(kernel_mir,
                         "Function name=timer.wake_fired_sleepers returns=[timer.TimerWakeResult]",
                         "phase102 merged MIR should keep timer wake delivery inside the timer helper boundary");
    ExpectOutputContains(kernel_mir,
                         "store_target target=TIMER_STATE target_kind=global target_name=TIMER_STATE",
                         "phase102 merged MIR should lower timer-state updates as global targets");
    ExpectOutputContains(kernel_mir,
                         "aggregate_init %v",
                         "phase102 merged MIR should use aggregate initialization for sleep and wake records");
    ExpectOutputContains(kernel_mir,
                         "variant_match",
                         "phase102 merged MIR should lower sleep and wake status classification through ordinary enum matching");
}

}  // namespace mc::tool_tests
