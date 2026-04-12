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

void ExpectPhase146BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase146_service_shape_consolidation_run_output.txt",
                                                             "freestanding kernel phase146 service shape consolidation run");
    ExpectExitCodeAtLeast(run_outcome,
                          146,
                          run_output,
                          "phase146 freestanding kernel service shape consolidation run should preserve the landed phase146 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_shell_service.mc.o")) {
        Fail("phase146 service shape consolidation should emit the shell_service module object");
    }
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_log_service.mc.o")) {
        Fail("phase146 service shape consolidation should emit the log_service module object");
    }
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_kv_service.mc.o")) {
        Fail("phase146 service shape consolidation should emit the kv_service module object");
    }
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_serial_service.mc.o")) {
        Fail("phase146 service shape consolidation should emit the serial_service module object");
    }
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_init.mc.o")) {
        Fail("phase146 service shape consolidation should emit the init module object");
    }
}

void ExpectPhase146PublicationSlice(const std::filesystem::path& phase_doc_path,
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
                         "Phase 146 -- Service Shape Consolidation",
                         "phase146 plan note should exist in canonical phase-doc style");
    ExpectOutputContains(phase_doc,
                         "Phase 146 is complete as one bounded service-shape consolidation step over the real kernel artifact.",
                         "phase146 plan note should close out the service-shape boundary explicitly");
    ExpectOutputContains(phase_doc,
                         "serial_service owns frame transport, shell_service owns shell syntax and routing, and peer services own service semantics",
                         "phase146 plan note should publish the shell-syntax versus service-semantics split explicitly");
    ExpectOutputContains(phase_doc,
                         "service framework, dynamic registration, or compiler expansion",
                         "phase146 plan note should keep framework and compiler widening non-goals explicit");

    const std::string roadmap = ReadFile(roadmap_path);
    ExpectOutputContains(roadmap,
                         "Phase 146 is now concrete as one bounded service-shape consolidation step",
                         "phase146 roadmap should record the landed service-shape boundary");

    const std::string position = ReadFile(position_path);
    ExpectOutputContains(position,
                         "landed Phase 146 service-shape consolidation step",
                         "phase146 position note should record the landed service-shape step");

    const std::string kernel_readme = ReadFile(kernel_readme_path);
    ExpectOutputContains(kernel_readme,
                         "bounded service-shape consolidation step",
                         "phase146 kernel README should describe the service-shape boundary");

    const std::string repo_map = ReadFile(repo_map_path);
    ExpectOutputContains(repo_map,
                         "phase146_service_shape_consolidation.cpp",
                         "phase146 repository map should list the new kernel proof owner");

    const std::string tool_readme = ReadFile(tool_readme_path);
    ExpectOutputContains(tool_readme,
                         "kernel/phase146_service_shape_consolidation.cpp",
                         "phase146 tool README should list the new kernel proof owner");

    const std::string freestanding_readme = ReadFile(freestanding_readme_path);
    ExpectOutputContains(freestanding_readme,
                         "phase146_service_shape_consolidation.cpp",
                         "phase146 freestanding README should list the new kernel proof owner");

    const std::string decision_log = ReadFile(decision_log_path);
    ExpectOutputContains(decision_log,
                         "Phase 146 service-shape consolidation",
                         "phase146 decision log should record the bounded service-shape decision");
    ExpectOutputContains(decision_log,
                         "keep compiler, sema, MIR, backend, ABI, target, runtime, and `hal` surfaces closed while publishing one explicit minimal common service shape from the landed shell, log, and key-value owners instead of introducing a framework",
                         "phase146 decision log should record the choice not to widen compiler or framework surfaces");

    const std::string backlog = ReadFile(backlog_path);
    ExpectOutputContains(backlog,
                         "keep the landed Phase 146 service-shape consolidation step explicit",
                         "phase146 backlog should keep the new service-shape boundary visible for later phases");
}

void ExpectPhase146MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "TypeDecl kind=struct name=debug.Phase146ServiceShapeConsolidationAudit",
            "Function name=bootstrap_audit.build_phase146_service_shape_consolidation_audit returns=[debug.Phase146ServiceShapeConsolidationAudit]",
            "Function name=serial_service.observe_composition returns=[serial_service.SerialCompositionObservation]",
            "Function name=debug.validate_phase146_service_shape_consolidation returns=[bool]",
        },
        expected_projection_path,
        "phase146 merged MIR should preserve the service-shape consolidation projection");
}

}  // namespace

void RunFreestandingKernelPhase146ServiceShapeConsolidation(const std::filesystem::path& source_root,
                                                            const std::filesystem::path& binary_root,
                                                            const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path phase_doc_path = source_root / "docs" / "plan" / "active" /
                                                 "phase146_service_shape_consolidation.txt";
    const std::filesystem::path roadmap_path = source_root / "docs" / "plan" / "admin" /
                                               "speculative_roadmap_post_phase145.txt";
    const std::filesystem::path position_path = source_root / "docs" / "plan" / "admin" /
                                                "modern_c_canopus_readiness_position.txt";
    const std::filesystem::path tool_readme_path = source_root / "tests" / "tool" / "README.md";
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase146_service_shape_consolidation.mirproj.txt";
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
                                                                 build_dir / "kernel_phase146_service_shape_consolidation_build_output.txt",
                                                                 "freestanding kernel phase146 service shape consolidation build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase146 freestanding kernel service shape consolidation build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase146BehaviorSlice(build_dir, build_targets);
    ExpectPhase146PublicationSlice(phase_doc_path,
                                   roadmap_path,
                                   position_path,
                                   common_paths.kernel_readme_path,
                                   common_paths.repo_map_path,
                                   tool_readme_path,
                                   common_paths.freestanding_readme_path,
                                   common_paths.decision_log_path,
                                   common_paths.backlog_path);
    ExpectPhase146MirStructureSlice(dump_targets.mir, mir_projection_path);
}

}  // namespace mc::tool_tests