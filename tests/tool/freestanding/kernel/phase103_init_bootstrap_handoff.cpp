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

void RunFreestandingKernelPhase103InitBootstrapHandoff(const std::filesystem::path& source_root,
                                                       const std::filesystem::path& binary_root,
                                                       const std::filesystem::path& mc_path) {
    const std::filesystem::path project_path = source_root / "kernel" / "build.toml";
    const std::filesystem::path main_source_path = source_root / "kernel" / "src/main.mc";
    const std::filesystem::path build_dir = binary_root / "kernel_init_bootstrap_handoff_build";
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
                                                                 build_dir / "kernel_init_bootstrap_handoff_build_output.txt",
                                                                 "freestanding kernel init-bootstrap-handoff build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase103 freestanding kernel init-bootstrap-handoff build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(main_source_path, build_dir);
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_init_bootstrap_handoff_run_output.txt",
                                                             "freestanding kernel init-bootstrap-handoff run");
    if (!run_outcome.exited || run_outcome.exit_code != 103) {
        Fail("phase103 freestanding kernel init-bootstrap-handoff run should exit with the handoff proof marker:\n" +
             run_output);
    }

    const std::string kernel_mir = ReadFile(dump_targets.mir);
    ExpectOutputContains(kernel_mir,
                         "TypeDecl kind=struct name=init.BootstrapCapabilitySet",
                         "phase103 merged MIR should preserve the imported bootstrap-capability-set type");
    ExpectOutputContains(kernel_mir,
                         "TypeDecl kind=struct name=init.BootstrapHandoffObservation",
                         "phase103 merged MIR should preserve the imported bootstrap-handoff observation type");
    ExpectOutputContains(kernel_mir,
                         "VarGlobal names=[INIT_BOOTSTRAP_CAPS] type=init.BootstrapCapabilitySet",
                         "phase103 merged MIR should retain the init bootstrap capability-set global");
    ExpectOutputContains(kernel_mir,
                         "VarGlobal names=[INIT_BOOTSTRAP_HANDOFF] type=init.BootstrapHandoffObservation",
                         "phase103 merged MIR should retain the init bootstrap-handoff observation global");
    ExpectOutputContains(kernel_mir,
                         "Function name=handoff_init_bootstrap_capability_set returns=[bool]",
                         "phase103 merged MIR should keep explicit bootstrap-cap handoff in the root proof module");
    ExpectOutputContains(kernel_mir,
                         "Function name=validate_init_bootstrap_capability_handoff returns=[bool]",
                         "phase103 merged MIR should keep bootstrap-cap validation in the root proof module");
    ExpectOutputContains(kernel_mir,
                         "Function name=init.install_bootstrap_capability_set returns=[init.BootstrapCapabilitySet]",
                         "phase103 merged MIR should keep bootstrap-cap installation inside the init helper boundary");
    ExpectOutputContains(kernel_mir,
                         "Function name=init.observe_bootstrap_handoff returns=[init.BootstrapHandoffObservation]",
                         "phase103 merged MIR should keep handoff observation construction inside the init helper boundary");
    ExpectOutputContains(kernel_mir,
                         "store_target target=INIT_BOOTSTRAP_CAPS target_kind=global target_name=INIT_BOOTSTRAP_CAPS",
                         "phase103 merged MIR should lower bootstrap-capability-set writes as global targets");
    ExpectOutputContains(kernel_mir,
                         "store_target target=INIT_BOOTSTRAP_HANDOFF target_kind=global target_name=INIT_BOOTSTRAP_HANDOFF",
                         "phase103 merged MIR should lower bootstrap-handoff writes as global targets");
    ExpectOutputContains(kernel_mir,
                         "aggregate_init %v",
                         "phase103 merged MIR should use aggregate initialization for bootstrap handoff records");
    ExpectOutputContains(kernel_mir,
                         "variant_match",
                         "phase103 merged MIR should lower bootstrap program-cap classification through ordinary enum matching");
}

}  // namespace mc::tool_tests