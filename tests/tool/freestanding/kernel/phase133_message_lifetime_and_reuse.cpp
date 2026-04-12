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

void ExpectPhase133BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase133_message_lifetime_run_output.txt",
                                                             "freestanding kernel phase133 message lifetime and reuse run");
    ExpectExitCodeAtLeast(run_outcome,
                          133,
                          run_output,
                          "phase133 freestanding kernel message lifetime and reuse run should preserve the landed phase133 slice");
}

void ExpectPhase133PublicationSlice(const std::filesystem::path& phase_doc_path,
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
                         "Phase 133 -- Message Lifetime And Reuse Audit",
                         "phase133 plan note should exist in canonical phase-doc style");
    ExpectOutputContains(phase_doc,
                         "Keep compiler, sema, MIR, backend, ABI, target, and `hal` surfaces closed.",
                         "phase133 plan note should record the decision not to widen compiler work");
    ExpectOutputContains(phase_doc,
                         "Phase 133 is complete as one bounded message lifetime and reuse audit.",
                         "phase133 plan note should close out the message-lifetime boundary explicitly");
    ExpectOutputContains(phase_doc,
                         "attached handle remains explicit across both delivery and both close and",
                         "phase133 plan note should describe the close and owner-death transfer-abort follow-through");
    ExpectOutputContains(phase_doc,
                         "Do not add a syscall-facing close surface in this phase.",
                         "phase133 plan note should record that endpoint close remains ipc-owned for now");

    const std::string roadmap = ReadFile(roadmap_path);
    ExpectOutputContains(roadmap,
                         "Phase 133 is now concrete as one bounded message lifetime and reuse audit",
                         "phase133 roadmap should record the landed lifetime boundary");

    const std::string position = ReadFile(position_path);
    ExpectOutputContains(position,
                         "landed Phase 133 message lifetime and reuse audit",
                         "phase133 position note should record the landed message-lifetime step");

    const std::string kernel_readme = ReadFile(kernel_readme_path);
    ExpectOutputContains(kernel_readme,
                         "bounded message lifetime and reuse audit",
                         "phase133 kernel README should describe the new message-lifetime boundary");

    const std::string repo_map = ReadFile(repo_map_path);
    ExpectOutputContains(repo_map,
                         "phase133_message_lifetime_and_reuse.cpp",
                         "phase133 repository map should list the new kernel proof owner");

    const std::string tool_readme = ReadFile(tool_readme_path);
    ExpectOutputContains(tool_readme,
                         "kernel/phase133_message_lifetime_and_reuse.cpp",
                         "phase133 tool README should list the new kernel proof owner");

    const std::string freestanding_readme = ReadFile(freestanding_readme_path);
    ExpectOutputContains(freestanding_readme,
                         "phase133_message_lifetime_and_reuse.cpp",
                         "phase133 freestanding README should list the new kernel proof owner");

    const std::string decision_log = ReadFile(decision_log_path);
    ExpectOutputContains(decision_log,
                         "Phase 133 message lifetime and reuse audit",
                         "phase133 decision log should record the limited-scope lifetime decision");
    ExpectOutputContains(decision_log,
                         "keep endpoint close as an ipc-owned internal action",
                         "phase133 decision log should record the choice not to add a syscall close surface");

    const std::string backlog = ReadFile(backlog_path);
    ExpectOutputContains(backlog,
                         "keep the Phase 133 message lifetime and reuse audit explicit",
                         "phase133 backlog should keep the new message-lifetime boundary visible for later phases");
    ExpectOutputContains(backlog,
                         "keep endpoint close bounded to the landed ipc-owned internal action",
                         "phase133 backlog should keep the close-surface boundary explicit");
}

void ExpectPhase133MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "TypeDecl kind=struct name=debug.Phase133MessageLifetimeAudit",
            "Function name=ipc.close_runtime_endpoint returns=[ipc.EndpointCloseTransition]",
            "Function name=ipc.close_runtime_endpoints_for_owner returns=[ipc.EndpointOwnerCloseTransition]",
            "Function name=debug.validate_phase133_message_lifetime_and_reuse returns=[bool]",
        },
        expected_projection_path,
        "phase133 merged MIR should preserve the message-lifetime projection");
}

}  // namespace

void RunFreestandingKernelPhase133MessageLifetimeAndReuse(const std::filesystem::path& source_root,
                                                          const std::filesystem::path& binary_root,
                                                          const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path phase_doc_path = source_root / "docs" / "plan" / "active" /
                                                 "phase133_message_lifetime_and_reuse_audit.txt";
    const std::filesystem::path position_path = source_root / "docs" / "plan" / "admin" /
                                                "modern_c_canopus_readiness_position.txt";
    const std::filesystem::path tool_readme_path = source_root / "tests" / "tool" / "README.md";
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase133_message_lifetime_and_reuse.mirproj.txt";
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
                                                                 build_dir / "kernel_phase133_message_lifetime_build_output.txt",
                                                                 "freestanding kernel phase133 message lifetime and reuse build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase133 freestanding kernel message lifetime and reuse build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase133BehaviorSlice(build_dir, build_targets);
    ExpectPhase133PublicationSlice(phase_doc_path,
                                   ResolveCanopusRoadmapPath(source_root, 133),
                                   position_path,
                                   common_paths.kernel_readme_path,
                                   common_paths.repo_map_path,
                                   tool_readme_path,
                                   common_paths.freestanding_readme_path,
                                   common_paths.decision_log_path,
                                   common_paths.backlog_path);
    ExpectPhase133MirStructureSlice(dump_targets.mir, mir_projection_path);
}

}  // namespace mc::tool_tests