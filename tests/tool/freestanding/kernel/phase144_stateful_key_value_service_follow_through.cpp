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

void ExpectPhase144BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase144_stateful_key_value_service_run_output.txt",
                                                             "freestanding kernel phase144 stateful key-value service run");
    ExpectExitCodeAtLeast(run_outcome,
                          144,
                          run_output,
                          "phase144 freestanding kernel stateful key-value service run should preserve the landed phase144 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_kv_service.mc.o")) {
        Fail("phase144 stateful key-value service should emit the kv_service module object");
    }
    if (!std::filesystem::exists(object_dir / "kernel__bootstrap_services.o")) {
        Fail("phase144 stateful key-value service should emit the bootstrap_services module object");
    }
}

void ExpectPhase144PublicationSlice(const std::filesystem::path& phase_doc_path,
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
                         "Phase 144 -- Stateful Key-Value Service Follow-Through",
                         "phase144 plan note should exist in canonical phase-doc style");
    ExpectOutputContains(phase_doc,
                         "Phase 144 is complete as one bounded stateful key-value service follow-through over the real kernel artifact.",
                         "phase144 plan note should close out the key-value boundary explicitly");
    ExpectOutputContains(phase_doc,
                         "accepts shell-routed `kv set <key> <value>` requests",
                         "phase144 plan note should publish the bounded set path explicitly");
    ExpectOutputContains(phase_doc,
                         "It does not claim a storage subsystem, a database abstraction layer, durable recovery, or general service middleware.",
                         "phase144 plan note should keep storage and middleware non-goals explicit");

    const std::string roadmap = ReadFile(roadmap_path);
    ExpectOutputContains(roadmap,
                         "Phase 144 is now concrete as one bounded stateful key-value service follow-through",
                         "phase144 roadmap should record the landed key-value step");

    const std::string position = ReadFile(position_path);
    ExpectOutputContains(position,
                         "landed Phase 144 stateful key-value service follow-through",
                         "phase144 position note should record the landed key-value boundary");

    const std::string kernel_readme = ReadFile(kernel_readme_path);
    ExpectOutputContains(kernel_readme,
                         "bounded stateful key-value service follow-through",
                         "phase144 kernel README should describe the retained key-value boundary");

    const std::string repo_map = ReadFile(repo_map_path);
    ExpectOutputContains(repo_map,
                         "phase144_stateful_key_value_service_follow_through.cpp",
                         "phase144 repository map should list the new kernel proof owner");

    const std::string tool_readme = ReadFile(tool_readme_path);
    ExpectOutputContains(tool_readme,
                         "kernel/phase144_stateful_key_value_service_follow_through.cpp",
                         "phase144 tool README should list the new kernel proof owner");

    const std::string freestanding_readme = ReadFile(freestanding_readme_path);
    ExpectOutputContains(freestanding_readme,
                         "phase144_stateful_key_value_service_follow_through.cpp",
                         "phase144 freestanding README should list the new kernel proof owner");

    const std::string decision_log = ReadFile(decision_log_path);
    ExpectOutputContains(decision_log,
                         "Phase 144 stateful key-value service follow-through",
                         "phase144 decision log should record the bounded key-value decision");
    ExpectOutputContains(decision_log,
                         "keep compiler, sema, MIR, backend, ABI, target, and `hal` surfaces closed while keeping key-value state bounded in-memory and routing one fixed key-value-write observation into `log_service`",
                         "phase144 decision log should record the choice not to widen compiler or storage surfaces");

    const std::string backlog = ReadFile(backlog_path);
    ExpectOutputContains(backlog,
                         "keep the landed Phase 144 stateful key-value service follow-through explicit",
                         "phase144 backlog should keep the bounded key-value boundary visible for later phases");
}

void ExpectPhase144MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "TypeDecl kind=struct name=debug.Phase144StatefulKeyValueServiceFollowThroughAudit",
            "Function name=bootstrap_audit.build_phase144_stateful_key_value_service_follow_through_audit returns=[debug.Phase144StatefulKeyValueServiceFollowThroughAudit]",
            "Function name=bootstrap_services.execute_phase144_stateful_key_value_shell_command returns=[bootstrap_services.Phase142ShellCommandRouteResult]",
            "Function name=debug.validate_phase144_stateful_key_value_service_follow_through returns=[bool]",
        },
        expected_projection_path,
        "phase144 merged MIR should preserve the stateful key-value-service projection");
}

}  // namespace

void RunFreestandingKernelPhase144StatefulKeyValueServiceFollowThrough(const std::filesystem::path& source_root,
                                                                       const std::filesystem::path& binary_root,
                                                                       const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path phase_doc_path = source_root / "docs" / "plan" / "active" /
                                                 "phase144_stateful_key_value_service_follow_through.txt";
    const std::filesystem::path position_path = source_root / "docs" / "plan" / "admin" /
                                                "modern_c_canopus_readiness_position.txt";
    const std::filesystem::path tool_readme_path = source_root / "tests" / "tool" / "README.md";
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase144_stateful_key_value_service_follow_through.mirproj.txt";
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
                                                                 build_dir / "kernel_phase144_stateful_key_value_service_build_output.txt",
                                                                 "freestanding kernel phase144 stateful key-value service build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase144 freestanding kernel stateful key-value service build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase144BehaviorSlice(build_dir, build_targets);
    ExpectPhase144PublicationSlice(phase_doc_path,
                                   ResolveCanopusRoadmapPath(source_root, 144),
                                   position_path,
                                   common_paths.kernel_readme_path,
                                   common_paths.repo_map_path,
                                   tool_readme_path,
                                   common_paths.freestanding_readme_path,
                                   common_paths.decision_log_path,
                                   common_paths.backlog_path);
    ExpectPhase144MirStructureSlice(dump_targets.mir, mir_projection_path);
}

}  // namespace mc::tool_tests