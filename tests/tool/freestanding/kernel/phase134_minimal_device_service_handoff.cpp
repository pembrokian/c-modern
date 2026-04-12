#include <filesystem>
#include <string>

#include "compiler/support/dump_paths.h"
#include "tests/support/process_utils.h"
#include "tests/tool/tool_suite_common.h"

namespace mc::tool_tests {

using mc::test_support::ExpectOutputContains;
using mc::test_support::Fail;
using mc::test_support::ReadFile;
using mc::test_support::RunCommandCapture;

namespace {

void ExpectPhase134BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase134_minimal_device_service_handoff_run_output.txt",
                                                             "freestanding kernel phase134 minimal device-service handoff run");
    ExpectExitCodeAtLeast(run_outcome,
                          134,
                          run_output,
                          "phase134 freestanding kernel minimal device-service handoff run should preserve the landed phase134 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_uart.mc.o")) {
        Fail("phase134 minimal device-service handoff should emit the uart module object");
    }
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_serial_service.mc.o")) {
        Fail("phase134 minimal device-service handoff should emit the serial_service module object");
    }
}

void ExpectPhase134PublicationSlice(const std::filesystem::path& phase_doc_path,
                                    const std::filesystem::path& roadmap_path,
                                    const std::filesystem::path& position_path,
                                    const std::filesystem::path& kernel_readme_path,
                                    const std::filesystem::path& repo_map_path,
                                    const std::filesystem::path& tool_readme_path,
                                    const std::filesystem::path& freestanding_readme_path,
                                    const std::filesystem::path& decision_log_path,
                                    const std::filesystem::path& backlog_path) {
    const std::string phase_doc = ReadFile(phase_doc_path);
    ExpectOutputContains(phase_doc,
                         "Phase 134 -- Minimal Device-Service Handoff",
                         "phase134 plan note should exist in canonical phase-doc style");
    ExpectOutputContains(phase_doc,
                         "Keep compiler, sema, MIR, backend, ABI, target, and `hal` surfaces closed.",
                         "phase134 plan note should record the decision not to widen compiler work");
    ExpectOutputContains(phase_doc,
                         "Phase 134 is complete as one bounded UART receive device-service handoff.",
                         "phase134 plan note should close out the device-service boundary explicitly");

    const std::string roadmap = ReadFile(roadmap_path);
    ExpectOutputContains(roadmap,
                         "Phase 134 is now concrete as one bounded UART receive device-service handoff",
                         "phase134 roadmap should record the landed device-service boundary");

    const std::string position = ReadFile(position_path);
    ExpectOutputContains(position,
                         "landed Phase 134 minimal device-service handoff",
                         "phase134 position note should record the landed device-service step");

    const std::string kernel_readme = ReadFile(kernel_readme_path);
    ExpectOutputContains(kernel_readme,
                         "bounded UART receive device-service handoff",
                         "phase134 kernel README should describe the new device-facing boundary");

    const std::string repo_map = ReadFile(repo_map_path);
    ExpectOutputContains(repo_map,
                         "phase134_minimal_device_service_handoff.cpp",
                         "phase134 repository map should list the new kernel proof owner");

    const std::string tool_readme = ReadFile(tool_readme_path);
    ExpectOutputContains(tool_readme,
                         "kernel/phase134_minimal_device_service_handoff.cpp",
                         "phase134 tool README should list the new kernel proof owner");

    const std::string freestanding_readme = ReadFile(freestanding_readme_path);
    ExpectOutputContains(freestanding_readme,
                         "phase134_minimal_device_service_handoff.cpp",
                         "phase134 freestanding README should list the new kernel proof owner");

    const std::string decision_log = ReadFile(decision_log_path);
    ExpectOutputContains(decision_log,
                         "Phase 134 minimal device-service handoff",
                         "phase134 decision log should record the limited-scope device-service decision");
    ExpectOutputContains(decision_log,
                         "no reusable driver framework",
                         "phase134 decision log should record the choice not to widen into a driver framework");

    const std::string backlog = ReadFile(backlog_path);
    ExpectOutputContains(backlog,
                         "keep the Phase 134 minimal device-service handoff explicit",
                         "phase134 backlog should keep the new device-service boundary visible for later phases");
}

void ExpectPhase134MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "TypeDecl kind=struct name=debug.Phase134MinimalDeviceServiceHandoffAudit",
            "Function name=ipc.publish_runtime_byte returns=[ipc.RuntimePublishResult]",
            "Function name=uart.handle_receive_interrupt returns=[uart.UartInterruptResult]",
            "Function name=bootstrap_services.execute_serial_service_receive_and_exit returns=[bootstrap_services.SerialServiceExecutionResult]",
            "Function name=debug.validate_phase134_minimal_device_service_handoff returns=[bool]",
        },
        expected_projection_path,
        "phase134 merged MIR should preserve the minimal device-service handoff projection");
}

}  // namespace

void RunFreestandingKernelPhase134MinimalDeviceServiceHandoff(const std::filesystem::path& source_root,
                                                              const std::filesystem::path& binary_root,
                                                              const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path phase_doc_path = source_root / "docs" / "plan" / "active" /
                                                 "phase134_minimal_device_service_handoff.txt";
    const std::filesystem::path position_path = source_root / "docs" / "plan" / "admin" /
                                                "modern_c_canopus_readiness_position.txt";
    const std::filesystem::path tool_readme_path = source_root / "tests" / "tool" / "README.md";
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase134_minimal_device_service_handoff.mirproj.txt";
    const std::filesystem::path build_dir = binary_root / "kernel_phase134_minimal_device_service_handoff_build";
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
                                                                 build_dir / "kernel_phase134_minimal_device_service_handoff_build_output.txt",
                                                                 "freestanding kernel phase134 minimal device-service handoff build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase134 freestanding kernel minimal device-service handoff build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase134BehaviorSlice(build_dir, build_targets);
    ExpectPhase134PublicationSlice(phase_doc_path,
                                   common_paths.roadmap_path,
                                   position_path,
                                   common_paths.kernel_readme_path,
                                   common_paths.repo_map_path,
                                   tool_readme_path,
                                   common_paths.freestanding_readme_path,
                                   common_paths.decision_log_path,
                                   common_paths.backlog_path);
    ExpectPhase134MirStructureSlice(dump_targets.mir, mir_projection_path);
}

}  // namespace mc::tool_tests