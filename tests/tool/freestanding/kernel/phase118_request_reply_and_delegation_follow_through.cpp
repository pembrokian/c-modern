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

void ExpectPhase118BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase118_delegation_run_output.txt",
                                                             "freestanding kernel phase118 request-reply and delegation run");
    ExpectExitCodeAtLeast(run_outcome,
                          118,
                          run_output,
                          "phase118 freestanding kernel delegated request-reply run should preserve the landed phase118 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_transfer_service.mc.o")) {
        Fail("phase118 delegated request-reply audit should emit the transfer_service module object");
    }
}

void ExpectPhase118PublicationSlice(const std::filesystem::path& phase_doc_path,
                                    const std::filesystem::path& roadmap_path,
                                    const std::filesystem::path& kernel_readme_path,
                                    const std::filesystem::path& repo_map_path,
                                    const std::filesystem::path& freestanding_readme_path,
                                    const std::filesystem::path& decision_log_path,
                                    const std::filesystem::path& backlog_path) {
    const std::string phase_doc = ReadFile(phase_doc_path);
    ExpectOutputContains(phase_doc,
                         "Phase 118 -- Request-Reply And Delegation Follow-Through",
                         "phase118 plan note should exist in canonical phase-doc style");
    ExpectOutputContains(phase_doc,
                         "Keep the compiler, sema, MIR, backend, ABI, and `hal` surfaces closed.",
                         "phase118 plan note should record the decision not to widen compiler work");
    ExpectOutputContains(phase_doc,
                         "Phase 118 is complete as a delegated request-reply follow-through.",
                         "phase118 plan note should close out the delegated running-system boundary explicitly");

    const std::string roadmap = ReadFile(roadmap_path);
    ExpectOutputContains(roadmap,
                         "Phase 118 is now concrete as one delegated request-reply and source-invalidation follow-through",
                         "phase118 roadmap should record the landed delegated running-system boundary");

    const std::string kernel_readme = ReadFile(kernel_readme_path);
    ExpectOutputContains(kernel_readme,
                         "bounded delegated request-reply follow-through",
                         "phase118 kernel README should describe the delegated reply boundary");

    const std::string repo_map = ReadFile(repo_map_path);
    ExpectOutputContains(repo_map,
                         "phase118_request_reply_and_delegation_follow_through.cpp",
                         "phase118 repository map should list the new kernel proof owner");

    const std::string freestanding_readme = ReadFile(freestanding_readme_path);
    ExpectOutputContains(freestanding_readme,
                         "phase118_request_reply_and_delegation_follow_through.cpp",
                         "phase118 freestanding README should list the new kernel proof owner");

    const std::string decision_log = ReadFile(decision_log_path);
    ExpectOutputContains(decision_log,
                         "Phase 118 request-reply and delegation follow-through",
                         "phase118 decision log should record the limited-scope delegation decision");

    const std::string backlog = ReadFile(backlog_path);
    ExpectOutputContains(backlog,
                         "keep the Phase 118 delegated request-reply follow-through explicit",
                         "phase118 backlog should keep the new running-system boundary visible for later phases");
}

void ExpectPhase118MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "TypeDecl kind=struct name=debug.Phase118DelegatedRequestReplyAudit",
            "Function name=bootstrap_audit.build_phase118_delegated_request_reply_audit returns=[debug.Phase118DelegatedRequestReplyAudit]",
            "Function name=debug.validate_phase118_request_reply_and_delegation_follow_through returns=[bool]",
        },
        expected_projection_path,
        "phase118 merged MIR should preserve the delegated request-reply projection");
}

}  // namespace

void RunFreestandingKernelPhase118RequestReplyAndDelegationFollowThrough(const std::filesystem::path& source_root,
                                                                         const std::filesystem::path& binary_root,
                                                                         const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path phase_doc_path = ResolvePlanDocPath(source_root,
                                                                    "phase118_request_reply_and_delegation_follow_through.txt");
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase118_request_reply_and_delegation_follow_through.mirproj.txt";
    const std::filesystem::path build_dir = binary_root / "kernel_phase118_delegation_build";
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
                                                                 build_dir / "kernel_phase118_delegation_build_output.txt",
                                                                 "freestanding kernel phase118 request-reply and delegation build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase118 freestanding kernel delegated request-reply build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase118BehaviorSlice(build_dir, build_targets);
    ExpectPhase118PublicationSlice(phase_doc_path,
                                   common_paths.roadmap_path,
                                   common_paths.kernel_readme_path,
                                   common_paths.repo_map_path,
                                   common_paths.freestanding_readme_path,
                                   common_paths.decision_log_path,
                                   common_paths.backlog_path);
    ExpectPhase118MirStructureSlice(dump_targets.mir, mir_projection_path);
}

}  // namespace mc::tool_tests