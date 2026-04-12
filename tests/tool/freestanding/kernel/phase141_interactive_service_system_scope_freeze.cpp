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

void ExpectPhase141BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase141_interactive_service_system_scope_freeze_run_output.txt",
                                                             "freestanding kernel phase141 interactive service system scope freeze run");
    ExpectExitCodeAtLeast(run_outcome,
                          141,
                          run_output,
                          "phase141 freestanding kernel interactive service system scope freeze run should preserve the landed phase141 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_shell_service.mc.o")) {
        Fail("phase141 interactive service system scope freeze should emit the shell_service module object");
    }
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_kv_service.mc.o")) {
        Fail("phase141 interactive service system scope freeze should emit the kv_service module object");
    }
}

void ExpectPhase141PublicationSlice(const std::filesystem::path& phase_doc_path,
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
                         "Phase 141 -- Interactive Service System Scope Freeze",
                         "phase141 plan note should exist in canonical phase-doc style");
    ExpectOutputContains(phase_doc,
                         "Phase 141 is complete as one bounded interactive service system scope freeze with concrete shell and key-value service owners.",
                         "phase141 plan note should close out the shell and kv owner boundary explicitly");
    ExpectOutputContains(phase_doc,
                         "UART -> serial_service -> shell_service -> { echo_service, log_service, kv_service }",
                         "phase141 plan note should publish the fixed shell-owned graph explicitly");
    ExpectOutputContains(phase_doc,
                         "It does not claim a shell framework, dynamic discovery, a broker, or compiler expansion.",
                         "phase141 plan note should keep the bounded ownership boundary explicit");

    const std::string roadmap = ReadFile(roadmap_path);
    ExpectOutputContains(roadmap,
                         "Phase 141 is now concrete as one bounded interactive service system scope freeze",
                         "phase141 roadmap should record the landed shell and kv owner freeze");

    const std::string position = ReadFile(position_path);
    ExpectOutputContains(position,
                         "landed Phase 141 interactive service system scope freeze",
                         "phase141 position note should record the landed shell-owned boundary");

    const std::string kernel_readme = ReadFile(kernel_readme_path);
    ExpectOutputContains(kernel_readme,
                         "bounded interactive service system scope freeze",
                         "phase141 kernel README should describe the new shell and kv boundary");

    const std::string repo_map = ReadFile(repo_map_path);
    ExpectOutputContains(repo_map,
                         "phase141_interactive_service_system_scope_freeze.cpp",
                         "phase141 repository map should list the new kernel proof owner");

    const std::string tool_readme = ReadFile(tool_readme_path);
    ExpectOutputContains(tool_readme,
                         "kernel/phase141_interactive_service_system_scope_freeze.cpp",
                         "phase141 tool README should list the new kernel proof owner");

    const std::string freestanding_readme = ReadFile(freestanding_readme_path);
    ExpectOutputContains(freestanding_readme,
                         "phase141_interactive_service_system_scope_freeze.cpp",
                         "phase141 freestanding README should list the new kernel proof owner");

    const std::string decision_log = ReadFile(decision_log_path);
    ExpectOutputContains(decision_log,
                         "Phase 141 interactive service system scope freeze",
                         "phase141 decision log should record the bounded shell and kv decision");
    ExpectOutputContains(decision_log,
                         "keep compiler, sema, MIR, backend, ABI, target, and `hal` surfaces closed while landing one concrete shell owner, one concrete key-value owner",
                         "phase141 decision log should record the choice not to widen compiler or framework work");

    const std::string backlog = ReadFile(backlog_path);
    ExpectOutputContains(backlog,
                         "keep the landed Phase 141 interactive service system scope freeze explicit",
                         "phase141 backlog should keep the shell and kv boundary visible for later phases");
}

void ExpectPhase141MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "TypeDecl kind=struct name=debug.Phase141InteractiveServiceSystemScopeFreezeAudit",
            "Function name=bootstrap_audit.build_phase141_interactive_service_system_scope_freeze_audit returns=[debug.Phase141InteractiveServiceSystemScopeFreezeAudit]",
            "Function name=debug.validate_phase141_interactive_service_system_scope_freeze returns=[bool]",
        },
        expected_projection_path,
        "phase141 merged MIR should preserve the interactive service system scope-freeze projection");
}

}  // namespace

void RunFreestandingKernelPhase141InteractiveServiceSystemScopeFreeze(const std::filesystem::path& source_root,
                                                                     const std::filesystem::path& binary_root,
                                                                     const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path phase_doc_path = source_root / "docs" / "plan" / "active" /
                                                 "phase141_interactive_service_system_scope_freeze.txt";
    const std::filesystem::path position_path = source_root / "docs" / "plan" / "admin" /
                                                "modern_c_canopus_readiness_position.txt";
    const std::filesystem::path tool_readme_path = source_root / "tests" / "tool" / "README.md";
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase141_interactive_service_system_scope_freeze.mirproj.txt";
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
                                                                 build_dir / "kernel_phase141_interactive_service_system_scope_freeze_build_output.txt",
                                                                 "freestanding kernel phase141 interactive service system scope freeze build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase141 freestanding kernel interactive service system scope freeze build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase141BehaviorSlice(build_dir, build_targets);
    ExpectPhase141PublicationSlice(phase_doc_path,
                                   ResolveCanopusRoadmapPath(source_root, 141),
                                   position_path,
                                   common_paths.kernel_readme_path,
                                   common_paths.repo_map_path,
                                   tool_readme_path,
                                   common_paths.freestanding_readme_path,
                                   common_paths.decision_log_path,
                                   common_paths.backlog_path);
    ExpectPhase141MirStructureSlice(dump_targets.mir, mir_projection_path);
}

}  // namespace mc::tool_tests