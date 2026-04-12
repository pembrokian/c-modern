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

void RunFreestandingKernelPhase98EndpointHandleCore(const std::filesystem::path& source_root,
                                                    const std::filesystem::path& binary_root,
                                                    const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path build_dir = binary_root / "kernel_endpoint_handle_core_build";
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
                                                                 build_dir / "kernel_endpoint_handle_core_build_output.txt",
                                                                 "freestanding kernel endpoint-handle-core build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase98 freestanding kernel endpoint-handle-core build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_endpoint_handle_core_run_output.txt",
                                                             "freestanding kernel endpoint-handle-core run");
    if (!run_outcome.exited || run_outcome.exit_code != 98) {
        Fail("phase98 freestanding kernel endpoint-handle-core run should exit with the endpoint-handle-core proof marker:\n" +
             run_output);
    }

    const std::string kernel_mir = ReadFile(dump_targets.mir);
    ExpectOutputContains(kernel_mir,
                         "TypeDecl kind=struct name=capability.HandleTable",
                         "phase98 merged MIR should preserve the imported handle-table type");
    ExpectOutputContains(kernel_mir,
                         "TypeDecl kind=struct name=endpoint.KernelMessage",
                         "phase98 merged MIR should preserve the imported kernel-message type");
    ExpectOutputContains(kernel_mir,
                         "TypeDecl kind=struct name=endpoint.EndpointSlot",
                         "phase98 merged MIR should preserve the imported endpoint-slot type");
    ExpectOutputContains(kernel_mir,
                         "VarGlobal names=[HANDLE_TABLES] type=[2]capability.HandleTable",
                         "phase98 merged MIR should retain the mutable per-process handle tables global");
    ExpectOutputContains(kernel_mir,
                         "VarGlobal names=[DELIVERED_MESSAGE] type=endpoint.KernelMessage",
                         "phase98 merged MIR should retain the delivered-message global");
    ExpectOutputContains(kernel_mir,
                         "Function name=bootstrap_endpoint_handle_core",
                         "phase98 merged MIR should keep explicit endpoint and handle-core construction in the root proof module");
    ExpectOutputContains(kernel_mir,
                         "Function name=endpoint.install_endpoint returns=[endpoint.EndpointTable]",
                         "phase98 merged MIR should keep endpoint installation inside the endpoint helper boundary");
    ExpectOutputContains(kernel_mir,
                         "Function name=capability.install_endpoint_handle returns=[capability.HandleTable]",
                         "phase98 merged MIR should keep handle installation inside the capability helper boundary");
    ExpectOutputContains(kernel_mir,
                         "store_target target=HANDLE_TABLES target_kind=global target_name=HANDLE_TABLES",
                         "phase98 merged MIR should lower handle-table writes as global targets");
    ExpectOutputContains(kernel_mir,
                         "store_target target=ENDPOINTS target_kind=global target_name=ENDPOINTS",
                         "phase98 merged MIR should lower endpoint-table writes as global targets");
    ExpectOutputContains(kernel_mir,
                         "store_target target=DELIVERED_MESSAGE target_kind=global target_name=DELIVERED_MESSAGE",
                         "phase98 merged MIR should lower delivered-message writes as global targets");
    ExpectOutputContains(kernel_mir,
                         "aggregate_init %v",
                         "phase98 merged MIR should use aggregate initialization for handle-table and message records");
    ExpectOutputContains(kernel_mir,
                         "variant_match",
                         "phase98 merged MIR should lower endpoint and handle classification through ordinary enum matching");
}

}  // namespace mc::tool_tests