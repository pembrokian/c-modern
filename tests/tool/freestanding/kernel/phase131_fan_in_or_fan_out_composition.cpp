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

void ExpectPhase131BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase131_fan_out_composition_run_output.txt",
                                                             "freestanding kernel phase131 fan-out composition run");
    ExpectExitCodeAtLeast(run_outcome,
                          131,
                          run_output,
                          "phase131 freestanding kernel fan-out composition run should preserve the landed phase131 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "kernel__bootstrap_audit.o")) {
        Fail("phase131 fan-out composition audit should emit the bootstrap_audit module object");
    }
}

void ExpectPhase131PublicationSlice(const std::filesystem::path& phase_doc_path,
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
                         "Phase 131 -- Fan-In Or Fan-Out Composition Probe",
                         "phase131 plan note should exist in canonical phase-doc style");
    ExpectOutputContains(phase_doc,
                         "Keep the compiler, sema, MIR, backend, ABI, and `hal` surfaces closed.",
                         "phase131 plan note should record the decision not to widen compiler work");
    ExpectOutputContains(phase_doc,
                         "Phase 131 is complete as one bounded fan-out composition probe.",
                         "phase131 plan note should close out the fan-out composition boundary explicitly");

    const std::string roadmap = ReadFile(roadmap_path);
    ExpectOutputContains(roadmap,
                         "Phase 131 is now concrete as one bounded fan-out composition probe",
                         "phase131 roadmap should record the landed composition boundary");

    const std::string position = ReadFile(position_path);
    ExpectOutputContains(position,
                         "landed Phase 131 fan-out composition probe",
                         "phase131 position note should record the landed composition step");

    const std::string kernel_readme = ReadFile(kernel_readme_path);
    ExpectOutputContains(kernel_readme,
                         "bounded fan-out composition probe",
                         "phase131 kernel README should describe the new IPC-composition boundary");

    const std::string repo_map = ReadFile(repo_map_path);
    ExpectOutputContains(repo_map,
                         "phase131_fan_in_or_fan_out_composition.cpp",
                         "phase131 repository map should list the new kernel proof owner");

    const std::string tool_readme = ReadFile(tool_readme_path);
    ExpectOutputContains(tool_readme,
                         "kernel/phase131_fan_in_or_fan_out_composition.cpp",
                         "phase131 tool README should list the new kernel proof owner");

    const std::string freestanding_readme = ReadFile(freestanding_readme_path);
    ExpectOutputContains(freestanding_readme,
                         "phase131_fan_in_or_fan_out_composition.cpp",
                         "phase131 freestanding README should list the new kernel proof owner");

    const std::string decision_log = ReadFile(decision_log_path);
    ExpectOutputContains(decision_log,
                         "Phase 131 fan-out composition probe",
                         "phase131 decision log should record the limited-scope composition decision");

    const std::string backlog = ReadFile(backlog_path);
    ExpectOutputContains(backlog,
                         "keep the Phase 131 fan-out composition probe explicit",
                         "phase131 backlog should keep the new composition boundary visible for later phases");
}

void ExpectPhase131MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "TypeDecl kind=struct name=debug.Phase131FanOutCompositionAudit",
            "Function name=bootstrap_audit.build_phase131_fan_out_composition_audit returns=[debug.Phase131FanOutCompositionAudit]",
            "Function name=debug.validate_phase131_fan_out_composition returns=[bool]",
        },
        expected_projection_path,
        "phase131 merged MIR should preserve the fan-out composition projection");
}

}  // namespace

void RunFreestandingKernelPhase131FanInOrFanOutComposition(const std::filesystem::path& source_root,
                                                           const std::filesystem::path& binary_root,
                                                           const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path phase_doc_path = ResolvePlanDocPath(source_root,
                                                                    "phase131_fan_in_or_fan_out_composition_probe.txt");
    const std::filesystem::path position_path = source_root / "docs" / "plan" / "admin" /
                                                "modern_c_canopus_readiness_position.txt";
    const std::filesystem::path tool_readme_path = source_root / "tests" / "tool" / "README.md";
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase131_fan_in_or_fan_out_composition.mirproj.txt";
    const std::filesystem::path build_dir = binary_root / "kernel_phase131_fan_out_composition_build";
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
                                                                 build_dir / "kernel_phase131_fan_out_composition_build_output.txt",
                                                                 "freestanding kernel phase131 fan-out composition build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase131 freestanding kernel fan-out composition build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase131BehaviorSlice(build_dir, build_targets);
    ExpectPhase131PublicationSlice(phase_doc_path,
                                   common_paths.roadmap_path,
                                   position_path,
                                   common_paths.kernel_readme_path,
                                   common_paths.repo_map_path,
                                   tool_readme_path,
                                   common_paths.freestanding_readme_path,
                                   common_paths.decision_log_path,
                                   common_paths.backlog_path);
    ExpectPhase131MirStructureSlice(dump_targets.mir, mir_projection_path);
}

}  // namespace mc::tool_tests