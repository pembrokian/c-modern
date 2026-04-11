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

void ExpectPhase119BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase119_namespace_run_output.txt",
                                                             "freestanding kernel phase119 namespace-pressure run");
    ExpectExitCodeAtLeast(run_outcome,
                          119,
                          run_output,
                          "phase119 freestanding kernel namespace-pressure run should preserve the landed phase119 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "kernel__bootstrap_audit.o")) {
        Fail("phase119 namespace-pressure audit should emit the bootstrap_audit module object");
    }
}

void ExpectPhase119PublicationSlice(const std::filesystem::path& phase_doc_path,
                                    const std::filesystem::path& roadmap_path,
                                    const std::filesystem::path& kernel_readme_path,
                                    const std::filesystem::path& repo_map_path,
                                    const std::filesystem::path& freestanding_readme_path,
                                    const std::filesystem::path& decision_log_path,
                                    const std::filesystem::path& backlog_path) {
    const std::string phase_doc = ReadFile(phase_doc_path);
    ExpectOutputContains(phase_doc,
                         "Phase 119 -- Namespace Pressure Audit",
                         "phase119 plan note should exist in canonical phase-doc style");
    ExpectOutputContains(phase_doc,
                         "Keep the compiler, sema, MIR, backend, ABI, and `hal` surfaces closed.",
                         "phase119 plan note should record the decision not to widen compiler work");
    ExpectOutputContains(phase_doc,
                         "Phase 119 is complete as a namespace-pressure audit.",
                         "phase119 plan note should close out the namespace-pressure boundary explicitly");

    const std::string roadmap = ReadFile(roadmap_path);
    ExpectOutputContains(roadmap,
                         "Phase 119 is now concrete as one init-owned fixed service-directory and namespace-pressure audit",
                         "phase119 roadmap should record the landed namespace-pressure boundary");

    const std::string kernel_readme = ReadFile(kernel_readme_path);
    ExpectOutputContains(kernel_readme,
                         "bounded init-owned fixed service-directory step",
                         "phase119 kernel README should describe the fixed namespace answer");

    const std::string repo_map = ReadFile(repo_map_path);
    ExpectOutputContains(repo_map,
                         "phase119_namespace_pressure_audit.cpp",
                         "phase119 repository map should list the new kernel proof owner");

    const std::string freestanding_readme = ReadFile(freestanding_readme_path);
    ExpectOutputContains(freestanding_readme,
                         "phase119_namespace_pressure_audit.cpp",
                         "phase119 freestanding README should list the new kernel proof owner");

    const std::string decision_log = ReadFile(decision_log_path);
    ExpectOutputContains(decision_log,
                         "Phase 119 namespace-pressure audit",
                         "phase119 decision log should record the limited-scope namespace decision");

    const std::string backlog = ReadFile(backlog_path);
    ExpectOutputContains(backlog,
                         "keep the Phase 119 namespace-pressure audit explicit",
                         "phase119 backlog should keep the new namespace boundary visible for later phases");
}

void ExpectPhase119MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "TypeDecl kind=struct name=debug.Phase119NamespacePressureAudit",
            "Function name=bootstrap_audit.build_phase119_namespace_pressure_audit returns=[debug.Phase119NamespacePressureAudit]",
            "Function name=debug.validate_phase119_namespace_pressure_audit returns=[bool]",
        },
        expected_projection_path,
        "phase119 merged MIR should preserve the namespace-pressure projection");
}

}  // namespace

void RunFreestandingKernelPhase119NamespacePressureAudit(const std::filesystem::path& source_root,
                                                         const std::filesystem::path& binary_root,
                                                         const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path phase_doc_path = source_root / "docs" / "plan" /
                                                 "phase119_namespace_pressure_audit.txt";
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase119_namespace_pressure_audit.mirproj.txt";
    const std::filesystem::path build_dir = binary_root / "kernel_phase119_namespace_build";
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
                                                                 build_dir / "kernel_phase119_namespace_build_output.txt",
                                                                 "freestanding kernel phase119 namespace-pressure build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase119 freestanding kernel namespace-pressure build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase119BehaviorSlice(build_dir, build_targets);
    ExpectPhase119PublicationSlice(phase_doc_path,
                                   common_paths.roadmap_path,
                                   common_paths.kernel_readme_path,
                                   common_paths.repo_map_path,
                                   common_paths.freestanding_readme_path,
                                   common_paths.decision_log_path,
                                   common_paths.backlog_path);
    ExpectPhase119MirStructureSlice(dump_targets.mir, mir_projection_path);
}

}  // namespace mc::tool_tests