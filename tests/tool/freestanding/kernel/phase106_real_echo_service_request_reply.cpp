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

void RunFreestandingKernelPhase106RealEchoServiceRequestReply(const std::filesystem::path& source_root,
                                                              const std::filesystem::path& binary_root,
                                                              const std::filesystem::path& mc_path) {
    const std::filesystem::path project_path = source_root / "kernel" / "build.toml";
    const std::filesystem::path main_source_path = source_root / "kernel" / "src/main.mc";
    const std::filesystem::path build_dir = binary_root / "kernel_phase106_echo_service_build";
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
                                                                 build_dir / "kernel_phase106_echo_service_build_output.txt",
                                                                 "freestanding kernel phase106 echo-service request-reply build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase106 freestanding kernel echo-service request-reply build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(main_source_path, build_dir);
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase106_echo_service_run_output.txt",
                                                             "freestanding kernel phase106 echo-service request-reply run");
    if (!run_outcome.exited || run_outcome.exit_code != 117) {
        Fail("phase106 freestanding kernel echo-service request-reply run should exit with the current kernel proof marker while preserving the phase106 slice:\n" +
             run_output);
    }

    const std::string kernel_mir = ReadFile(dump_targets.mir);
    ExpectOutputContains(kernel_mir,
                         "Function name=validate_phase105_log_service_handshake returns=[bool]",
                         "phase106 merged MIR should preserve the landed phase105 log-service proof");
    ExpectOutputContains(kernel_mir,
                         "TypeDecl kind=struct name=echo_service.EchoServiceState",
                         "phase106 merged MIR should declare the bounded echo-service state record");
    ExpectOutputContains(kernel_mir,
                         "TypeDecl kind=struct name=echo_service.EchoExchangeObservation",
                         "phase106 merged MIR should declare the bounded echo-service exchange observation record");
    ExpectOutputContains(kernel_mir,
                         "Function name=execute_phase106_echo_service_request_reply returns=[bool]",
                         "phase106 merged MIR should keep the real echo-service proof in the root kernel module");
    ExpectOutputContains(kernel_mir,
                         "Function name=echo_service.record_request returns=[echo_service.EchoServiceState]",
                         "phase106 merged MIR should keep request observation inside the echo-service helper boundary");
    ExpectOutputContains(kernel_mir,
                         "Function name=echo_service.reply_payload returns=[[4]u8]",
                         "phase106 merged MIR should keep reply-payload construction inside the echo-service helper boundary");
    ExpectOutputContains(kernel_mir,
                         "Function name=echo_service.observe_exchange returns=[echo_service.EchoExchangeObservation]",
                         "phase106 merged MIR should keep the final exchange projection inside the echo-service helper boundary");
    ExpectOutputContains(kernel_mir,
                         "Function name=validate_phase106_echo_service_request_reply returns=[bool]",
                         "phase106 merged MIR should retain the end-to-end validation path for the real echo-service slice");
}

}  // namespace mc::tool_tests