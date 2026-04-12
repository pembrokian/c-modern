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

void ExpectPhase136BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase136_device_failure_containment_run_output.txt",
                                                             "freestanding kernel phase136 device failure containment run");
    ExpectExitCodeAtLeast(run_outcome,
                          136,
                          run_output,
                          "phase136 freestanding kernel device failure containment run should preserve the landed phase136 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_uart.mc.o")) {
        Fail("phase136 device failure containment probe should emit the uart module object");
    }
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_serial_service.mc.o")) {
        Fail("phase136 device failure containment probe should emit the serial_service module object");
    }
}

void ExpectPhase136PublicationSlice(const std::filesystem::path& phase_doc_path,
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
                         "Phase 136 -- Device Failure Containment Probe",
                         "phase136 plan note should exist in canonical phase-doc style");
    ExpectOutputContains(phase_doc,
                         "Phase 136 is complete as one bounded device failure containment probe.",
                         "phase136 plan note should close out the containment step explicitly");
    ExpectOutputContains(phase_doc,
                         "one bounded UART queue-full drop observation",
                         "phase136 plan note should record the bounded queue-full consequence");
    ExpectOutputContains(phase_doc,
                         "It does not claim recovery supervision, retry frameworks, flow-control",
                         "phase136 plan note should record the choice not to widen into recovery policy");

    const std::string roadmap = ReadFile(roadmap_path);
    ExpectOutputContains(roadmap,
                         "Phase 136 is now concrete as one bounded device failure containment probe",
                         "phase136 roadmap should record the landed containment step");

    const std::string position = ReadFile(position_path);
    ExpectOutputContains(position,
                         "landed Phase 136 device failure containment probe",
                         "phase136 position note should record the landed containment step");

    const std::string kernel_readme = ReadFile(kernel_readme_path);
    ExpectOutputContains(kernel_readme,
                         "bounded device failure containment probe",
                         "phase136 kernel README should describe the new unhappy-path boundary");

    const std::string repo_map = ReadFile(repo_map_path);
    ExpectOutputContains(repo_map,
                         "phase136_device_failure_containment_probe.cpp",
                         "phase136 repository map should list the new kernel proof owner");

    const std::string tool_readme = ReadFile(tool_readme_path);
    ExpectOutputContains(tool_readme,
                         "kernel/phase136_device_failure_containment_probe.cpp",
                         "phase136 tool README should list the new kernel proof owner");

    const std::string freestanding_readme = ReadFile(freestanding_readme_path);
    ExpectOutputContains(freestanding_readme,
                         "phase136_device_failure_containment_probe.cpp",
                         "phase136 freestanding README should list the new kernel proof owner");

    const std::string decision_log = ReadFile(decision_log_path);
    ExpectOutputContains(decision_log,
                         "Phase 136 device failure containment probe",
                         "phase136 decision log should record the limited-scope containment decision");
    ExpectOutputContains(decision_log,
                         "one service-local malformed-byte classification path plus one bounded queue-full drop plus one explicit endpoint-close consequence",
                         "phase136 decision log should record the choice not to widen into supervision or retry work");

    const std::string backlog = ReadFile(backlog_path);
    ExpectOutputContains(backlog,
                         "keep the Phase 136 device failure containment probe explicit",
                         "phase136 backlog should keep the containment boundary visible for later phases");
}

void ExpectPhase136MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "TypeDecl kind=struct name=debug.Phase136DeviceFailureContainmentAudit",
            "Function name=uart.handle_receive_interrupt returns=[uart.UartInterruptResult]",
            "Function name=serial_service.record_ingress returns=[serial_service.SerialServiceState]",
            "Function name=bootstrap_services.close_serial_service_after_failure returns=[bootstrap_services.SerialServiceExecutionResult]",
            "Function name=debug.validate_phase136_device_failure_containment returns=[bool]",
        },
        expected_projection_path,
        "phase136 merged MIR should preserve the containment-boundary projection");
}

}  // namespace

void RunFreestandingKernelPhase136DeviceFailureContainmentProbe(const std::filesystem::path& source_root,
                                                                const std::filesystem::path& binary_root,
                                                                const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path phase_doc_path = source_root / "docs" / "plan" / "active" /
                                                 "phase136_device_failure_containment_probe.txt";
    const std::filesystem::path position_path = source_root / "docs" / "plan" / "admin" /
                                                "modern_c_canopus_readiness_position.txt";
    const std::filesystem::path tool_readme_path = source_root / "tests" / "tool" / "README.md";
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase136_device_failure_containment_probe.mirproj.txt";
    const std::filesystem::path build_dir = binary_root / "kernel_phase136_device_failure_containment_build";
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
                                                                 build_dir / "kernel_phase136_device_failure_containment_build_output.txt",
                                                                 "freestanding kernel phase136 device failure containment build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase136 freestanding kernel device failure containment build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase136BehaviorSlice(build_dir, build_targets);
    ExpectPhase136PublicationSlice(phase_doc_path,
                                   common_paths.roadmap_path,
                                   position_path,
                                   common_paths.kernel_readme_path,
                                   common_paths.repo_map_path,
                                   tool_readme_path,
                                   common_paths.freestanding_readme_path,
                                   common_paths.decision_log_path,
                                   common_paths.backlog_path);
    ExpectPhase136MirStructureSlice(dump_targets.mir, mir_projection_path);
}

}  // namespace mc::tool_tests
