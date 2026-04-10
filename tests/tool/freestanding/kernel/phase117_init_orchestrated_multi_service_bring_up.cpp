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

void ExpectPhase117BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase117_multi_service_run_output.txt",
                                                             "freestanding kernel phase117 multi-service run");
    if (!run_outcome.exited || run_outcome.exit_code != 118) {
        Fail("phase117 freestanding kernel multi-service run should exit with the current kernel proof marker:\n" +
             run_output);
    }

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_bootstrap_services.mc.o")) {
        Fail("phase117 multi-service audit should emit the bootstrap_services module object");
    }
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_log_service.mc.o")) {
        Fail("phase117 multi-service audit should emit the log_service module object");
    }
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_echo_service.mc.o")) {
        Fail("phase117 multi-service audit should emit the echo_service module object");
    }
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_transfer_service.mc.o")) {
        Fail("phase117 multi-service audit should emit the transfer_service module object");
    }
}

void ExpectPhase117PublicationSlice(const std::filesystem::path& phase_doc_path,
                                    const std::filesystem::path& roadmap_path,
                                    const std::filesystem::path& position_path,
                                    const std::filesystem::path& kernel_readme_path,
                                    const std::filesystem::path& repo_map_path,
                                    const std::filesystem::path& freestanding_readme_path,
                                    const std::filesystem::path& decision_log_path,
                                    const std::filesystem::path& backlog_path) {
    const std::string phase_doc = ReadFile(phase_doc_path);
    ExpectOutputContains(phase_doc,
                         "Phase 117 -- Init-Orchestrated Multi-Service Bring-Up",
                         "phase117 plan note should exist in canonical phase-doc style");
    ExpectOutputContains(phase_doc,
                         "Keep the compiler, sema, MIR, backend, ABI, and `hal` surfaces closed.",
                         "phase117 plan note should record the decision not to widen compiler work");
    ExpectOutputContains(phase_doc,
                         "Phase 117 is complete as an init-orchestrated multi-service bring-up.",
                         "phase117 plan note should close out the aggregate running-system boundary explicitly");

    const std::string roadmap = ReadFile(roadmap_path);
    ExpectOutputContains(roadmap,
                         "Phase 117 is now concrete as one init-orchestrated multi-service bring-up",
                         "phase117 roadmap should record the landed running-system boundary");

    const std::string position = ReadFile(position_path);
    ExpectOutputContains(position,
                         "after Phase 118 landed the delegated request-reply follow-through",
                         "phase117 position note should advance the current repository position");
    ExpectOutputContains(position,
                         "landed Phase 118 delegated request-reply follow-through.",
                         "phase117 position note should reference the new closeout");

    const std::string kernel_readme = ReadFile(kernel_readme_path);
    ExpectOutputContains(kernel_readme,
                         "Phase 118 has moved the repository-owned kernel artifact beyond the landed",
                         "phase117 kernel README should record the new running-system status");
    ExpectOutputContains(kernel_readme,
                         "bounded init-orchestrated multi-service bring-up",
                         "phase117 kernel README should describe the aggregate init-owned service set");

    const std::string repo_map = ReadFile(repo_map_path);
    ExpectOutputContains(repo_map,
                         "currently a Phase 118 delegated-request-reply kernel target",
                         "phase117 repository map should describe the current kernel boundary");
    ExpectOutputContains(repo_map,
                         "phase117_init_orchestrated_multi_service_bring_up.cpp",
                         "phase117 repository map should list the new kernel proof owner");

    const std::string freestanding_readme = ReadFile(freestanding_readme_path);
    ExpectOutputContains(freestanding_readme,
                         "phase117_init_orchestrated_multi_service_bring_up.cpp",
                         "phase117 freestanding README should list the new kernel proof owner");

    const std::string decision_log = ReadFile(decision_log_path);
    ExpectOutputContains(decision_log,
                         "Phase 117 init-orchestrated multi-service bring-up",
                         "phase117 decision log should record the limited-scope init-owned decision");

    const std::string backlog = ReadFile(backlog_path);
    ExpectOutputContains(backlog,
                         "keep the Phase 117 init-orchestrated multi-service bring-up explicit",
                         "phase117 backlog should keep the new running-system boundary visible for later phases");
}

void ExpectPhase117MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "ConstGlobal names=[PHASE118_MARKER] type=i32",
            "TypeDecl kind=struct name=debug.Phase117MultiServiceBringUpAudit",
            "Function name=bootstrap_audit.build_phase117_multi_service_bring_up_audit returns=[debug.Phase117MultiServiceBringUpAudit]",
            "Function name=debug.validate_phase117_init_orchestrated_multi_service_bring_up returns=[bool]",
        },
        expected_projection_path,
        "phase117 merged MIR should preserve the aggregate multi-service projection");
}

}  // namespace

void RunFreestandingKernelPhase117InitOrchestratedMultiServiceBringUp(const std::filesystem::path& source_root,
                                                                      const std::filesystem::path& binary_root,
                                                                      const std::filesystem::path& mc_path) {
    const std::filesystem::path project_path = source_root / "kernel" / "build.toml";
    const std::filesystem::path main_source_path = source_root / "kernel" / "src" / "main.mc";
    const std::filesystem::path phase_doc_path = source_root / "docs" / "plan" /
                                                 "phase117_init_orchestrated_multi_service_bring_up.txt";
    const std::filesystem::path roadmap_path = source_root / "docs" / "plan" / "admin" /
                                               "canopus_post_phase109_speculative_roadmap.txt";
    const std::filesystem::path position_path = source_root / "docs" / "plan" / "admin" /
                                                "modern_c_canopus_readiness_position.txt";
    const std::filesystem::path kernel_readme_path = source_root / "kernel" / "README.md";
    const std::filesystem::path repo_map_path = source_root / "docs" / "agent" / "prompts" / "repo_map.md";
    const std::filesystem::path freestanding_readme_path = source_root / "tests" / "tool" / "freestanding" / "README.md";
    const std::filesystem::path decision_log_path = source_root / "docs" / "plan" / "decision_log.txt";
    const std::filesystem::path backlog_path = source_root / "docs" / "plan" / "backlog.txt";
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase117_init_orchestrated_multi_service_bring_up.mirproj.txt";
    const std::filesystem::path build_dir = binary_root / "kernel_phase117_multi_service_build";
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
                                                                 build_dir / "kernel_phase117_multi_service_build_output.txt",
                                                                 "freestanding kernel phase117 multi-service build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase117 freestanding kernel multi-service build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(main_source_path, build_dir);
    ExpectPhase117BehaviorSlice(build_dir, build_targets);
    ExpectPhase117PublicationSlice(phase_doc_path,
                                   roadmap_path,
                                   position_path,
                                   kernel_readme_path,
                                   repo_map_path,
                                   freestanding_readme_path,
                                   decision_log_path,
                                   backlog_path);
    ExpectPhase117MirStructureSlice(dump_targets.mir, mir_projection_path);
}

}  // namespace mc::tool_tests