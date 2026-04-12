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

void ExpectPhase147BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase147_ipc_shape_audit_under_real_usage_run_output.txt",
                                                             "freestanding kernel phase147 ipc shape audit under real usage run");
    ExpectExitCodeAtLeast(run_outcome,
                          147,
                          run_output,
                          "phase147 freestanding kernel ipc shape audit under real usage run should preserve the landed phase147 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_shell_service.mc.o")) {
        Fail("phase147 ipc shape audit under real usage should emit the shell_service module object");
    }
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_log_service.mc.o")) {
        Fail("phase147 ipc shape audit under real usage should emit the log_service module object");
    }
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_kv_service.mc.o")) {
        Fail("phase147 ipc shape audit under real usage should emit the kv_service module object");
    }
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_serial_service.mc.o")) {
        Fail("phase147 ipc shape audit under real usage should emit the serial_service module object");
    }
    if (!std::filesystem::exists(object_dir / "kernel__bootstrap_services.o")) {
        Fail("phase147 ipc shape audit under real usage should emit the bootstrap_services module object");
    }
}

void ExpectPhase147PublicationSlice(const std::filesystem::path& phase_doc_path,
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
                         "Phase 147 -- IPC Shape Audit Under Real Usage",
                         "phase147 plan note should exist in canonical phase-doc style");
    ExpectOutputContains(phase_doc,
                         "Phase 147 is complete as one bounded IPC-shape audit under repeated real usage of the landed interactive-service slice.",
                         "phase147 plan note should close out the repeated-usage ipc boundary explicitly");
    ExpectOutputContains(phase_doc,
                         "The landed repeated shell workflow confirms that the current compact shell-plus-service message vocabulary remains sufficient",
                         "phase147 plan note should publish the sufficiency judgment explicitly");
    ExpectOutputContains(phase_doc,
                         "It does not claim dynamic payload typing, a generic message bus, a general serialization layer, or compiler expansion.",
                         "phase147 plan note should keep bus and compiler widening non-goals explicit");

    const std::string roadmap = ReadFile(roadmap_path);
    ExpectOutputContains(roadmap,
                         "Phase 147 is now concrete as one bounded IPC-shape audit under real usage",
                         "phase147 roadmap should record the landed IPC-shape boundary");

    const std::string position = ReadFile(position_path);
    ExpectOutputContains(position,
                         "landed Phase 147 IPC-shape audit under real usage",
                         "phase147 position note should record the landed IPC-shape step");

    const std::string kernel_readme = ReadFile(kernel_readme_path);
    ExpectOutputContains(kernel_readme,
                         "bounded IPC-shape audit under real usage",
                         "phase147 kernel README should describe the repeated-usage IPC boundary");

    const std::string repo_map = ReadFile(repo_map_path);
    ExpectOutputContains(repo_map,
                         "phase147_ipc_shape_audit_under_real_usage.cpp",
                         "phase147 repository map should list the new kernel proof owner");

    const std::string tool_readme = ReadFile(tool_readme_path);
    ExpectOutputContains(tool_readme,
                         "kernel/phase147_ipc_shape_audit_under_real_usage.cpp",
                         "phase147 tool README should list the new kernel proof owner");

    const std::string freestanding_readme = ReadFile(freestanding_readme_path);
    ExpectOutputContains(freestanding_readme,
                         "phase147_ipc_shape_audit_under_real_usage.cpp",
                         "phase147 freestanding README should list the new kernel proof owner");

    const std::string decision_log = ReadFile(decision_log_path);
    ExpectOutputContains(decision_log,
                         "Phase 147 IPC-shape audit under real usage",
                         "phase147 decision log should record the bounded IPC-shape decision");
    ExpectOutputContains(decision_log,
                         "keep compiler, sema, MIR, backend, ABI, target, runtime, and `hal` surfaces closed while confirming the current compact shell-plus-log-plus-key-value vocabulary is sufficient under repeated mixed command traffic",
                         "phase147 decision log should record the choice not to widen compiler or messaging surfaces");

    const std::string backlog = ReadFile(backlog_path);
    ExpectOutputContains(backlog,
                         "keep the landed Phase 147 IPC-shape audit under real usage explicit",
                         "phase147 backlog should keep the new repeated-usage IPC boundary visible for later phases");
}

void ExpectPhase147MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "TypeDecl kind=struct name=debug.Phase147IpcShapeAudit",
            "Function name=bootstrap_audit.build_phase147_ipc_shape_audit returns=[debug.Phase147IpcShapeAudit]",
            "Function name=bootstrap_services.phase147_ipc_shape_workflow_result returns=[bootstrap_services.Phase147IpcShapeWorkflowResult]",
            "Function name=debug.validate_phase147_ipc_shape_audit_under_real_usage returns=[bool]",
        },
        expected_projection_path,
        "phase147 merged MIR should preserve the repeated-usage IPC-shape projection");
}

}  // namespace

void RunFreestandingKernelPhase147IpcShapeAuditUnderRealUsage(const std::filesystem::path& source_root,
                                                              const std::filesystem::path& binary_root,
                                                              const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path phase_doc_path = source_root / "docs" / "plan" / "active" /
                                                 "phase147_ipc_shape_audit_under_real_usage.txt";
    const std::filesystem::path roadmap_path = source_root / "docs" / "plan" / "admin" /
                                               "speculative_roadmap_post_phase145.txt";
    const std::filesystem::path position_path = source_root / "docs" / "plan" / "admin" /
                                                "modern_c_canopus_readiness_position.txt";
    const std::filesystem::path tool_readme_path = source_root / "tests" / "tool" / "README.md";
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase147_ipc_shape_audit_under_real_usage.mirproj.txt";
    const std::filesystem::path build_dir = binary_root / "kernel_build";
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
                                                                 build_dir / "kernel_phase147_ipc_shape_audit_under_real_usage_build_output.txt",
                                                                 "freestanding kernel phase147 ipc shape audit under real usage build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase147 freestanding kernel ipc shape audit under real usage build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase147BehaviorSlice(build_dir, build_targets);
    ExpectPhase147PublicationSlice(phase_doc_path,
                                   roadmap_path,
                                   position_path,
                                   common_paths.kernel_readme_path,
                                   common_paths.repo_map_path,
                                   tool_readme_path,
                                   common_paths.freestanding_readme_path,
                                   common_paths.decision_log_path,
                                   common_paths.backlog_path);
    ExpectPhase147MirStructureSlice(dump_targets.mir, mir_projection_path);
}

}  // namespace mc::tool_tests