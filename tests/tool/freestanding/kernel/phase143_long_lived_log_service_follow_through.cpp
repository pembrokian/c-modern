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

void ExpectPhase143BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase143_long_lived_log_service_run_output.txt",
                                                             "freestanding kernel phase143 long-lived log service run");
    ExpectExitCodeAtLeast(run_outcome,
                          143,
                          run_output,
                          "phase143 freestanding kernel long-lived log service run should preserve the landed phase143 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_log_service.mc.o")) {
        Fail("phase143 long-lived log service should emit the log_service module object");
    }
}

void ExpectPhase143PublicationSlice(const std::filesystem::path& phase_doc_path,
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
                         "Phase 143 -- Long-Lived Log Service Follow-Through",
                         "phase143 plan note should exist in canonical phase-doc style");
    ExpectOutputContains(phase_doc,
                         "Phase 143 is complete as one bounded long-lived log-service follow-through over the real kernel artifact.",
                         "phase143 plan note should close out the retained log boundary explicitly");
    ExpectOutputContains(phase_doc,
                         "retain a bounded in-memory ordered log across multiple requests",
                         "phase143 plan note should publish the retained log boundary explicitly");
    ExpectOutputContains(phase_doc,
                         "It does not claim durable persistence, a filesystem, an indexing engine, or compiler expansion.",
                         "phase143 plan note should keep persistence and compiler non-goals explicit");

    const std::string roadmap = ReadFile(roadmap_path);
    ExpectOutputContains(roadmap,
                         "Phase 143 is now concrete as one bounded long-lived log-service follow-through",
                         "phase143 roadmap should record the landed retained-log step");

    const std::string position = ReadFile(position_path);
    ExpectOutputContains(position,
                         "landed Phase 143 long-lived log-service follow-through",
                         "phase143 position note should record the landed retained-log boundary");

    const std::string kernel_readme = ReadFile(kernel_readme_path);
    ExpectOutputContains(kernel_readme,
                         "bounded long-lived log-service follow-through",
                         "phase143 kernel README should describe the retained-log boundary");

    const std::string repo_map = ReadFile(repo_map_path);
    ExpectOutputContains(repo_map,
                         "phase143_long_lived_log_service_follow_through.cpp",
                         "phase143 repository map should list the new kernel proof owner");

    const std::string tool_readme = ReadFile(tool_readme_path);
    ExpectOutputContains(tool_readme,
                         "kernel/phase143_long_lived_log_service_follow_through.cpp",
                         "phase143 tool README should list the new kernel proof owner");

    const std::string freestanding_readme = ReadFile(freestanding_readme_path);
    ExpectOutputContains(freestanding_readme,
                         "phase143_long_lived_log_service_follow_through.cpp",
                         "phase143 freestanding README should list the new kernel proof owner");

    const std::string decision_log = ReadFile(decision_log_path);
    ExpectOutputContains(decision_log,
                         "Phase 143 long-lived log service follow-through",
                         "phase143 decision log should record the bounded retained-log decision");
    ExpectOutputContains(decision_log,
                         "keep compiler, sema, MIR, backend, ABI, target, and `hal` surfaces closed while keeping log retention bounded and in-memory",
                         "phase143 decision log should record the choice not to widen compiler or persistence surfaces");

    const std::string backlog = ReadFile(backlog_path);
    ExpectOutputContains(backlog,
                         "keep the landed Phase 143 long-lived log-service follow-through explicit",
                         "phase143 backlog should keep the retained-log boundary visible for later phases");
}

void ExpectPhase143MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "TypeDecl kind=struct name=debug.Phase143LongLivedLogServiceAudit",
            "Function name=bootstrap_audit.build_phase143_long_lived_log_service_audit returns=[debug.Phase143LongLivedLogServiceAudit]",
            "Function name=log_service.observe_retention returns=[log_service.LogRetentionObservation]",
            "Function name=debug.validate_phase143_long_lived_log_service returns=[bool]",
        },
        expected_projection_path,
        "phase143 merged MIR should preserve the long-lived log-service projection");
}

}  // namespace

void RunFreestandingKernelPhase143LongLivedLogServiceFollowThrough(const std::filesystem::path& source_root,
                                                                   const std::filesystem::path& binary_root,
                                                                   const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path phase_doc_path = source_root / "docs" / "plan" / "active" /
                                                 "phase143_long_lived_log_service_follow_through.txt";
    const std::filesystem::path position_path = source_root / "docs" / "plan" / "admin" /
                                                "modern_c_canopus_readiness_position.txt";
    const std::filesystem::path tool_readme_path = source_root / "tests" / "tool" / "README.md";
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase143_long_lived_log_service_follow_through.mirproj.txt";
    const std::filesystem::path build_dir = binary_root / "kernel_phase143_long_lived_log_service_build";
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
                                                                 build_dir / "kernel_phase143_long_lived_log_service_build_output.txt",
                                                                 "freestanding kernel phase143 long-lived log service build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase143 freestanding kernel long-lived log service build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase143BehaviorSlice(build_dir, build_targets);
    ExpectPhase143PublicationSlice(phase_doc_path,
                                   common_paths.roadmap_path,
                                   position_path,
                                   common_paths.kernel_readme_path,
                                   common_paths.repo_map_path,
                                   tool_readme_path,
                                   common_paths.freestanding_readme_path,
                                   common_paths.decision_log_path,
                                   common_paths.backlog_path);
    ExpectPhase143MirStructureSlice(dump_targets.mir, mir_projection_path);
}

}  // namespace mc::tool_tests