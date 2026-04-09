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

void RunFreestandingKernelPhase101SpawnWait(const std::filesystem::path& source_root,
                                            const std::filesystem::path& binary_root,
                                            const std::filesystem::path& mc_path) {
    const std::filesystem::path project_path = source_root / "kernel" / "build.toml";
    const std::filesystem::path main_source_path = source_root / "kernel" / "src/main.mc";
    const std::filesystem::path build_dir = binary_root / "kernel_spawn_wait_build";
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
                                                                 build_dir / "kernel_spawn_wait_build_output.txt",
                                                                 "freestanding kernel spawn-wait build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase101 freestanding kernel spawn-wait build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(main_source_path, build_dir);
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_spawn_wait_run_output.txt",
                                                             "freestanding kernel spawn-wait run");
    if (!run_outcome.exited || run_outcome.exit_code != 101) {
        Fail("phase101 freestanding kernel spawn-wait run should exit with the spawn-wait proof marker:\n" +
             run_output);
    }

    const std::string kernel_mir = ReadFile(dump_targets.mir);
    ExpectOutputContains(kernel_mir,
                         "TypeDecl kind=struct name=syscall.SpawnRequest",
                         "phase101 merged MIR should preserve the imported spawn-request type");
    ExpectOutputContains(kernel_mir,
                         "TypeDecl kind=struct name=syscall.WaitObservation",
                         "phase101 merged MIR should preserve the imported wait-observation type");
    ExpectOutputContains(kernel_mir,
                         "VarGlobal names=[SPAWN_OBSERVATION] type=syscall.SpawnObservation",
                         "phase101 merged MIR should retain the spawn observation global");
    ExpectOutputContains(kernel_mir,
                         "VarGlobal names=[PRE_EXIT_WAIT_OBSERVATION] type=syscall.WaitObservation",
                         "phase101 merged MIR should retain the pre-exit wait observation global");
    ExpectOutputContains(kernel_mir,
                         "VarGlobal names=[EXIT_WAIT_OBSERVATION] type=syscall.WaitObservation",
                         "phase101 merged MIR should retain the exit wait observation global");
    ExpectOutputContains(kernel_mir,
                         "VarGlobal names=[CHILD_ADDRESS_SPACE] type=address_space.AddressSpace",
                         "phase101 merged MIR should retain the child address-space global");
    ExpectOutputContains(kernel_mir,
                         "Function name=execute_program_cap_spawn_and_wait returns=[bool]",
                         "phase101 merged MIR should keep explicit spawn-and-wait execution in the root proof module");
    ExpectOutputContains(kernel_mir,
                         "Function name=validate_program_cap_spawn_and_wait returns=[bool]",
                         "phase101 merged MIR should keep explicit spawn-and-wait validation in the root proof module");
    ExpectOutputContains(kernel_mir,
                         "Function name=syscall.perform_spawn returns=[syscall.SpawnResult]",
                         "phase101 merged MIR should keep program-cap spawn inside the syscall helper boundary");
    ExpectOutputContains(kernel_mir,
                         "Function name=syscall.perform_wait returns=[syscall.WaitResult]",
                         "phase101 merged MIR should keep wait observation inside the syscall helper boundary");
    ExpectOutputContains(kernel_mir,
                         "Function name=capability.install_wait_handle returns=[capability.WaitTable]",
                         "phase101 merged MIR should keep wait-handle installation inside the capability helper boundary");
    ExpectOutputContains(kernel_mir,
                         "store_target target=WAIT_TABLES target_kind=global target_name=WAIT_TABLES",
                         "phase101 merged MIR should lower wait-table updates as global targets");
    ExpectOutputContains(kernel_mir,
                         "store_target target=INIT_PROGRAM_CAPABILITY target_kind=global target_name=INIT_PROGRAM_CAPABILITY",
                         "phase101 merged MIR should lower consumed program-capability state as a global target");
    ExpectOutputContains(kernel_mir,
                         "aggregate_init %v",
                         "phase101 merged MIR should use aggregate initialization for spawn and wait records");
    ExpectOutputContains(kernel_mir,
                         "variant_match",
                         "phase101 merged MIR should lower spawn and wait status classification through ordinary enum matching");
}

}  // namespace mc::tool_tests