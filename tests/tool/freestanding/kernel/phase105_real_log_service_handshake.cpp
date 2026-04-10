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

void RunFreestandingKernelPhase105RealLogServiceHandshake(const std::filesystem::path& source_root,
                                                          const std::filesystem::path& binary_root,
                                                          const std::filesystem::path& mc_path) {
    const std::filesystem::path project_path = source_root / "kernel" / "build.toml";
    const std::filesystem::path main_source_path = source_root / "kernel" / "src/main.mc";
    const std::filesystem::path build_dir = binary_root / "kernel_phase105_log_service_build";
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
                                                                 build_dir / "kernel_phase105_log_service_build_output.txt",
                                                                 "freestanding kernel phase105 log-service handshake build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase105 freestanding kernel log-service handshake build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(main_source_path, build_dir);
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase105_log_service_run_output.txt",
                                                             "freestanding kernel phase105 log-service handshake run");
    if (!run_outcome.exited || run_outcome.exit_code != 114) {
        Fail("phase105 freestanding kernel log-service handshake run should exit with the current kernel proof marker while preserving the phase105 slice:\n" +
             run_output);
    }

    const std::string kernel_mir = ReadFile(dump_targets.mir);
    ExpectOutputContains(kernel_mir,
                         "Function name=validate_phase104_contract_hardening returns=[bool]",
                         "phase105 merged MIR should preserve the landed phase104 hardening proof");
    ExpectOutputContains(kernel_mir,
                         "TypeDecl kind=struct name=log_service.LogServiceState",
                         "phase105 merged MIR should declare the bounded log-service state record");
    ExpectOutputContains(kernel_mir,
                         "TypeDecl kind=struct name=log_service.LogHandshakeObservation",
                         "phase105 merged MIR should declare the bounded log-service handshake observation record");
    ExpectOutputContains(kernel_mir,
                         "Function name=execute_phase105_log_service_handshake returns=[bool]",
                         "phase105 merged MIR should keep the real log-service proof in the root kernel module");
    ExpectOutputContains(kernel_mir,
                         "Function name=log_service.record_open_request returns=[log_service.LogServiceState]",
                         "phase105 merged MIR should keep request observation inside the log-service helper boundary");
    ExpectOutputContains(kernel_mir,
                         "Function name=log_service.observe_handshake returns=[log_service.LogHandshakeObservation]",
                         "phase105 merged MIR should keep the final handshake projection inside the log-service helper boundary");
    ExpectOutputContains(kernel_mir,
                         "Function name=log_service.ack_payload returns=[[4]u8]",
                         "phase105 merged MIR should build the acknowledgment payload through ordinary fixed-array operations");
    ExpectOutputContains(kernel_mir,
                         "Function name=validate_phase105_log_service_handshake returns=[bool]",
                         "phase105 merged MIR should retain the end-to-end validation path for the real log-service slice");
}

}  // namespace mc::tool_tests