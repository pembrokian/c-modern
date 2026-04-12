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

void ExpectPhase135BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase135_buffer_ownership_boundary_run_output.txt",
                                                             "freestanding kernel phase135 buffer ownership boundary run");
    ExpectExitCodeAtLeast(run_outcome,
                          135,
                          run_output,
                          "phase135 freestanding kernel buffer ownership boundary run should preserve the landed phase135 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_uart.mc.o")) {
        Fail("phase135 buffer ownership boundary should emit the uart module object");
    }
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_serial_service.mc.o")) {
        Fail("phase135 buffer ownership boundary should emit the serial_service module object");
    }
}

void ExpectPhase135PublicationSlice(const std::filesystem::path& phase_doc_path,
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
                         "Phase 135 -- Buffer Ownership Boundary Audit",
                         "phase135 plan note should exist in canonical phase-doc style");
    ExpectOutputContains(phase_doc,
                         "Choose the boring design on purpose for this phase: copy semantics.",
                         "phase135 plan note should record copy semantics as the ownership baseline");
    ExpectOutputContains(phase_doc,
                         "Phase 135 is complete as one bounded buffer ownership boundary audit.",
                         "phase135 plan note should close out the ownership boundary explicitly");
    ExpectOutputContains(phase_doc,
                         "Do not introduce shared ring buffers, general buffer pools, or a reusable",
                         "phase135 plan note should record the choice not to widen into buffer frameworks");

    const std::string roadmap = ReadFile(roadmap_path);
    ExpectOutputContains(roadmap,
                         "Phase 135 is now concrete as one bounded buffer ownership boundary audit",
                         "phase135 roadmap should record the landed ownership boundary");

    const std::string position = ReadFile(position_path);
    ExpectOutputContains(position,
                         "landed Phase 135 buffer ownership boundary audit",
                         "phase135 position note should record the landed ownership step");

    const std::string kernel_readme = ReadFile(kernel_readme_path);
    ExpectOutputContains(kernel_readme,
                         "bounded UART receive-frame ownership boundary audit",
                         "phase135 kernel README should describe the new ownership boundary");

    const std::string repo_map = ReadFile(repo_map_path);
    ExpectOutputContains(repo_map,
                         "phase135_buffer_ownership_boundary_audit.cpp",
                         "phase135 repository map should list the new kernel proof owner");

    const std::string tool_readme = ReadFile(tool_readme_path);
    ExpectOutputContains(tool_readme,
                         "kernel/phase135_buffer_ownership_boundary_audit.cpp",
                         "phase135 tool README should list the new kernel proof owner");

    const std::string freestanding_readme = ReadFile(freestanding_readme_path);
    ExpectOutputContains(freestanding_readme,
                         "phase135_buffer_ownership_boundary_audit.cpp",
                         "phase135 freestanding README should list the new kernel proof owner");

    const std::string decision_log = ReadFile(decision_log_path);
    ExpectOutputContains(decision_log,
                         "Phase 135 buffer ownership boundary audit",
                         "phase135 decision log should record the limited-scope ownership decision");
    ExpectOutputContains(decision_log,
                         "copy semantics as the explicit ownership baseline",
                         "phase135 decision log should record the choice not to widen into zero-copy or pooling work");

    const std::string backlog = ReadFile(backlog_path);
    ExpectOutputContains(backlog,
                         "keep the Phase 135 buffer ownership boundary audit explicit",
                         "phase135 backlog should keep the new ownership boundary visible for later phases");
}

void ExpectPhase135MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "TypeDecl kind=struct name=debug.Phase135BufferOwnershipAudit",
            "Function name=ipc.publish_runtime_frame returns=[ipc.RuntimePublishResult]",
            "Function name=uart.stage_receive_frame returns=[uart.UartDevice]",
            "Function name=bootstrap_services.execute_serial_service_receive returns=[bootstrap_services.SerialServiceExecutionResult]",
            "Function name=debug.validate_phase135_buffer_ownership_boundary returns=[bool]",
        },
        expected_projection_path,
        "phase135 merged MIR should preserve the ownership-boundary projection");
}

}  // namespace

void RunFreestandingKernelPhase135BufferOwnershipBoundaryAudit(const std::filesystem::path& source_root,
                                                               const std::filesystem::path& binary_root,
                                                               const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path phase_doc_path = source_root / "docs" / "plan" / "active" /
                                                 "phase135_buffer_ownership_boundary_audit.txt";
    const std::filesystem::path position_path = source_root / "docs" / "plan" / "admin" /
                                                "modern_c_canopus_readiness_position.txt";
    const std::filesystem::path tool_readme_path = source_root / "tests" / "tool" / "README.md";
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase135_buffer_ownership_boundary_audit.mirproj.txt";
    const std::filesystem::path build_dir = binary_root / "kernel_phase135_buffer_ownership_boundary_build";
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
                                                                 build_dir / "kernel_phase135_buffer_ownership_boundary_build_output.txt",
                                                                 "freestanding kernel phase135 buffer ownership boundary build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase135 freestanding kernel buffer ownership boundary build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase135BehaviorSlice(build_dir, build_targets);
    ExpectPhase135PublicationSlice(phase_doc_path,
                                   common_paths.roadmap_path,
                                   position_path,
                                   common_paths.kernel_readme_path,
                                   common_paths.repo_map_path,
                                   tool_readme_path,
                                   common_paths.freestanding_readme_path,
                                   common_paths.decision_log_path,
                                   common_paths.backlog_path);
    ExpectPhase135MirStructureSlice(dump_targets.mir, mir_projection_path);
}

}  // namespace mc::tool_tests
