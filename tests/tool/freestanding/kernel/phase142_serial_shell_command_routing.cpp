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

void ExpectPhase142BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase142_serial_shell_command_routing_run_output.txt",
                                                             "freestanding kernel phase142 serial shell command routing run");
    ExpectExitCodeAtLeast(run_outcome,
                          142,
                          run_output,
                          "phase142 freestanding kernel serial shell command routing run should preserve the landed phase142 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_shell_service.mc.o")) {
        Fail("phase142 serial shell command routing should emit the shell_service module object");
    }
    if (!std::filesystem::exists(object_dir / "kernel__bootstrap_services.o")) {
        Fail("phase142 serial shell command routing should emit the bootstrap_services module object");
    }
}

void ExpectPhase142PublicationSlice(const std::filesystem::path& phase_doc_path,
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
                         "Phase 142 -- Serial Shell Command Routing",
                         "phase142 plan note should exist in canonical phase-doc style");
    ExpectOutputContains(phase_doc,
                         "Phase 142 is complete as one bounded serial shell command-routing step over the real kernel artifact.",
                         "phase142 plan note should close out the serial shell command-routing boundary explicitly");
    ExpectOutputContains(phase_doc,
                         "ECxy -> echo <text>",
                         "phase142 plan note should publish the compact echo command encoding explicitly");
    ExpectOutputContains(phase_doc,
                         "It does not claim a wider serial payload, a generic shell framework, dynamic discovery, or compiler expansion.",
                         "phase142 plan note should keep the bounded no-payload-growth boundary explicit");

    const std::string roadmap = ReadFile(roadmap_path);
    ExpectOutputContains(roadmap,
                         "Phase 142 is now concrete as one bounded serial shell command-routing step",
                         "phase142 roadmap should record the landed shell command-routing step");

    const std::string position = ReadFile(position_path);
    ExpectOutputContains(position,
                         "landed Phase 142 serial shell command routing",
                         "phase142 position note should record the landed shell command-routing boundary");

    const std::string kernel_readme = ReadFile(kernel_readme_path);
    ExpectOutputContains(kernel_readme,
                         "bounded serial shell command-routing step",
                         "phase142 kernel README should describe the new shell command-routing boundary");

    const std::string repo_map = ReadFile(repo_map_path);
    ExpectOutputContains(repo_map,
                         "phase142_serial_shell_command_routing.cpp",
                         "phase142 repository map should list the new kernel proof owner");

    const std::string tool_readme = ReadFile(tool_readme_path);
    ExpectOutputContains(tool_readme,
                         "kernel/phase142_serial_shell_command_routing.cpp",
                         "phase142 tool README should list the new kernel proof owner");

    const std::string freestanding_readme = ReadFile(freestanding_readme_path);
    ExpectOutputContains(freestanding_readme,
                         "phase142_serial_shell_command_routing.cpp",
                         "phase142 freestanding README should list the new kernel proof owner");

    const std::string decision_log = ReadFile(decision_log_path);
    ExpectOutputContains(decision_log,
                         "Phase 142 serial shell command routing",
                         "phase142 decision log should record the bounded shell command-routing decision");
    ExpectOutputContains(decision_log,
                         "keep compiler, sema, MIR, backend, ABI, target, and `hal` surfaces closed while keeping the serial and IPC payload width fixed at four bytes",
                         "phase142 decision log should record the choice not to widen compiler or payload width");

    const std::string backlog = ReadFile(backlog_path);
    ExpectOutputContains(backlog,
                         "keep the landed Phase 142 serial shell command-routing step explicit",
                         "phase142 backlog should keep the shell command-routing boundary visible for later phases");
}

void ExpectPhase142MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "TypeDecl kind=struct name=debug.Phase142SerialShellCommandRoutingAudit",
            "Function name=bootstrap_audit.build_phase142_serial_shell_command_routing_audit returns=[debug.Phase142SerialShellCommandRoutingAudit]",
            "Function name=bootstrap_services.execute_phase142_shell_command returns=[bootstrap_services.Phase142ShellCommandRouteResult]",
            "Function name=debug.validate_phase142_serial_shell_command_routing returns=[bool]",
        },
        expected_projection_path,
        "phase142 merged MIR should preserve the serial shell command-routing projection");
}

}  // namespace

void RunFreestandingKernelPhase142SerialShellCommandRouting(const std::filesystem::path& source_root,
                                                            const std::filesystem::path& binary_root,
                                                            const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path phase_doc_path = source_root / "docs" / "plan" / "active" /
                                                 "phase142_serial_shell_command_routing.txt";
    const std::filesystem::path position_path = source_root / "docs" / "plan" / "admin" /
                                                "modern_c_canopus_readiness_position.txt";
    const std::filesystem::path tool_readme_path = source_root / "tests" / "tool" / "README.md";
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase142_serial_shell_command_routing.mirproj.txt";
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
                                                                 build_dir / "kernel_phase142_serial_shell_command_routing_build_output.txt",
                                                                 "freestanding kernel phase142 serial shell command routing build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase142 freestanding kernel serial shell command routing build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase142BehaviorSlice(build_dir, build_targets);
    ExpectPhase142PublicationSlice(phase_doc_path,
                                   ResolveCanopusRoadmapPath(source_root, 142),
                                   position_path,
                                   common_paths.kernel_readme_path,
                                   common_paths.repo_map_path,
                                   tool_readme_path,
                                   common_paths.freestanding_readme_path,
                                   common_paths.decision_log_path,
                                   common_paths.backlog_path);
    ExpectPhase142MirStructureSlice(dump_targets.mir, mir_projection_path);
}

}  // namespace mc::tool_tests