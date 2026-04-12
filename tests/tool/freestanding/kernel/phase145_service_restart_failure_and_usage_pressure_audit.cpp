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

void ExpectPhase145BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase145_service_restart_failure_and_usage_pressure_run_output.txt",
                                                             "freestanding kernel phase145 service restart failure and usage pressure run");
    ExpectExitCodeAtLeast(run_outcome,
                          145,
                          run_output,
                          "phase145 freestanding kernel service restart failure and usage pressure run should preserve the landed phase145 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_kv_service.mc.o")) {
        Fail("phase145 service restart failure and usage pressure audit should emit the kv_service module object");
    }
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_init.mc.o")) {
        Fail("phase145 service restart failure and usage pressure audit should emit the init module object");
    }
    if (!std::filesystem::exists(object_dir / "kernel__bootstrap_services.o")) {
        Fail("phase145 service restart failure and usage pressure audit should emit the bootstrap_services module object");
    }
}

void ExpectPhase145PublicationSlice(const std::filesystem::path& phase_doc_path,
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
                         "Phase 145 -- Service Restart, Failure, And Usage Pressure Audit",
                         "phase145 plan note should exist in canonical phase-doc style");
    ExpectOutputContains(phase_doc,
                         "Phase 145 is complete as one bounded service restart, failure, and usage pressure audit over the real kernel artifact.",
                         "phase145 plan note should close out the restart-and-usage boundary explicitly");
    ExpectOutputContains(phase_doc,
                         "manual retry remains a user-space-visible awkwardness after restart",
                         "phase145 plan note should record the landed manual-retry consequence honestly");
    ExpectOutputContains(phase_doc,
                         "kernel supervisor, durable recovery, transparent state",
                         "phase145 plan note should keep supervision and durability non-goals explicit");

    const std::string roadmap = ReadFile(roadmap_path);
    ExpectOutputContains(roadmap,
                         "Phase 145 is now concrete as one bounded service restart, failure, and usage pressure audit",
                         "phase145 roadmap should record the landed failure-and-restart boundary");

    const std::string position = ReadFile(position_path);
    ExpectOutputContains(position,
                         "landed Phase 145 service restart, failure, and usage pressure audit",
                         "phase145 position note should record the landed restart-and-usage step");

    const std::string kernel_readme = ReadFile(kernel_readme_path);
    ExpectOutputContains(kernel_readme,
                         "bounded service restart, failure, and usage pressure audit",
                         "phase145 kernel README should describe the restart-and-usage boundary");

    const std::string repo_map = ReadFile(repo_map_path);
    ExpectOutputContains(repo_map,
                         "phase145_service_restart_failure_and_usage_pressure_audit.cpp",
                         "phase145 repository map should list the new kernel proof owner");

    const std::string tool_readme = ReadFile(tool_readme_path);
    ExpectOutputContains(tool_readme,
                         "kernel/phase145_service_restart_failure_and_usage_pressure_audit.cpp",
                         "phase145 tool README should list the new kernel proof owner");

    const std::string freestanding_readme = ReadFile(freestanding_readme_path);
    ExpectOutputContains(freestanding_readme,
                         "phase145_service_restart_failure_and_usage_pressure_audit.cpp",
                         "phase145 freestanding README should list the new kernel proof owner");

    const std::string decision_log = ReadFile(decision_log_path);
    ExpectOutputContains(decision_log,
                         "Phase 145 service restart, failure, and usage pressure audit",
                         "phase145 decision log should record the bounded restart-pressure decision");
    ExpectOutputContains(decision_log,
                         "keep compiler, sema, MIR, backend, ABI, target, and `hal` surfaces closed while routing one explicit shell-visible key-value unavailable consequence, one init-owned fixed-wiring restart, and one explicit post-restart empty-state truth",
                         "phase145 decision log should record the choice not to widen compiler or supervision surfaces");

    const std::string backlog = ReadFile(backlog_path);
    ExpectOutputContains(backlog,
                         "keep the landed Phase 145 service restart, failure, and usage pressure audit explicit",
                         "phase145 backlog should keep the new restart-and-usage boundary visible for later phases");
}

void ExpectPhase145MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "TypeDecl kind=struct name=debug.Phase145ServiceRestartFailureAndUsagePressureAudit",
            "Function name=bootstrap_audit.build_phase145_service_restart_failure_and_usage_pressure_audit returns=[debug.Phase145ServiceRestartFailureAndUsagePressureAudit]",
            "Function name=bootstrap_services.execute_phase145_service_restart_shell_command returns=[bootstrap_services.Phase142ShellCommandRouteResult]",
            "Function name=debug.validate_phase145_service_restart_failure_and_usage_pressure returns=[bool]",
        },
        expected_projection_path,
        "phase145 merged MIR should preserve the service-restart failure-and-usage projection");
}

}  // namespace

void RunFreestandingKernelPhase145ServiceRestartFailureAndUsagePressureAudit(const std::filesystem::path& source_root,
                                                                             const std::filesystem::path& binary_root,
                                                                             const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path phase_doc_path = source_root / "docs" / "plan" / "active" /
                                                 "phase145_service_restart_failure_and_usage_pressure_audit.txt";
    const std::filesystem::path position_path = source_root / "docs" / "plan" / "admin" /
                                                "modern_c_canopus_readiness_position.txt";
    const std::filesystem::path tool_readme_path = source_root / "tests" / "tool" / "README.md";
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase145_service_restart_failure_and_usage_pressure_audit.mirproj.txt";
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
                                                                 build_dir / "kernel_phase145_service_restart_failure_and_usage_pressure_build_output.txt",
                                                                 "freestanding kernel phase145 service restart failure and usage pressure build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase145 freestanding kernel service restart failure and usage pressure build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase145BehaviorSlice(build_dir, build_targets);
    ExpectPhase145PublicationSlice(phase_doc_path,
                                   ResolveCanopusRoadmapPath(source_root, 145),
                                   position_path,
                                   common_paths.kernel_readme_path,
                                   common_paths.repo_map_path,
                                   tool_readme_path,
                                   common_paths.freestanding_readme_path,
                                   common_paths.decision_log_path,
                                   common_paths.backlog_path);
    ExpectPhase145MirStructureSlice(dump_targets.mir, mir_projection_path);
}

}  // namespace mc::tool_tests