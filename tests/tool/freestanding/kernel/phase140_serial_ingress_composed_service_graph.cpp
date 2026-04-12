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

void ExpectPhase140BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase140_serial_ingress_composed_service_graph_run_output.txt",
                                                             "freestanding kernel phase140 serial-ingress composed service graph run");
    ExpectExitCodeAtLeast(run_outcome,
                          140,
                          run_output,
                          "phase140 freestanding kernel serial-ingress composed service graph run should preserve the landed phase140 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_serial_service.mc.o")) {
        Fail("phase140 serial-ingress composed service graph should emit the serial_service module object");
    }
    if (!std::filesystem::exists(object_dir / "kernel__bootstrap_audit.o")) {
        Fail("phase140 serial-ingress composed service graph should emit the bootstrap_audit module object");
    }
}

void ExpectPhase140PublicationSlice(const std::filesystem::path& phase_doc_path,
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
                         "Phase 140 -- Serial-Ingress Composed Service Graph",
                         "phase140 plan note should exist in canonical phase-doc style");
    ExpectOutputContains(phase_doc,
                         "Phase 140 is complete as one bounded serial-ingress composed service graph.",
                         "phase140 plan note should close out the serial-ingress graph explicitly");
    ExpectOutputContains(phase_doc,
                         "UART -> serial_service -> composition_service -> { echo_service, log_service }",
                         "phase140 plan note should publish the fixed composed graph explicitly");
    ExpectOutputContains(phase_doc,
                         "It does not claim a broker, dynamic routing, or a general service-graph subsystem.",
                         "phase140 plan note should keep the no-broker boundary explicit");

    const std::string roadmap = ReadFile(roadmap_path);
    ExpectOutputContains(roadmap,
                         "Phase 140 is now concrete as one bounded serial-ingress composed service graph",
                         "phase140 roadmap should record the landed composed graph");

    const std::string position = ReadFile(position_path);
    ExpectOutputContains(position,
                         "landed Phase 140 serial-ingress composed service graph",
                         "phase140 position note should record the landed composed graph");

    const std::string kernel_readme = ReadFile(kernel_readme_path);
    ExpectOutputContains(kernel_readme,
                         "bounded serial-ingress composed service graph",
                         "phase140 kernel README should describe the new composed routing boundary");

    const std::string repo_map = ReadFile(repo_map_path);
    ExpectOutputContains(repo_map,
                         "phase140_serial_ingress_composed_service_graph.cpp",
                         "phase140 repository map should list the new kernel proof owner");

    const std::string tool_readme = ReadFile(tool_readme_path);
    ExpectOutputContains(tool_readme,
                         "kernel/phase140_serial_ingress_composed_service_graph.cpp",
                         "phase140 tool README should list the new kernel proof owner");

    const std::string freestanding_readme = ReadFile(freestanding_readme_path);
    ExpectOutputContains(freestanding_readme,
                         "phase140_serial_ingress_composed_service_graph.cpp",
                         "phase140 freestanding README should list the new kernel proof owner");

    const std::string decision_log = ReadFile(decision_log_path);
    ExpectOutputContains(decision_log,
                         "Phase 140 serial-ingress composed service graph",
                         "phase140 decision log should record the limited-scope composed-graph decision");
    ExpectOutputContains(decision_log,
                         "keep compiler, sema, MIR, backend, ABI, target, and `hal` surfaces closed while expressing the richer graph as one serial-service-owned forward plus one reused composition-service fan-out",
                         "phase140 decision log should record the choice not to widen compiler or broker work");

    const std::string backlog = ReadFile(backlog_path);
    ExpectOutputContains(backlog,
                         "keep the landed Phase 140 serial-ingress composed service graph explicit",
                         "phase140 backlog should keep the composed-graph boundary visible for later phases");
}

void ExpectPhase140MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "TypeDecl kind=struct name=debug.Phase140SerialIngressComposedServiceGraphAudit",
            "Function name=bootstrap_audit.build_phase140_serial_ingress_composed_service_graph_audit returns=[debug.Phase140SerialIngressComposedServiceGraphAudit]",
            "Function name=bootstrap_services.execute_phase140_serial_ingress_composed_service_graph returns=[bootstrap_services.Phase140SerialIngressCompositionResult]",
            "Function name=debug.validate_phase140_serial_ingress_composed_service_graph returns=[bool]",
        },
        expected_projection_path,
        "phase140 merged MIR should preserve the serial-ingress composed graph projection");
}

}  // namespace

void RunFreestandingKernelPhase140SerialIngressComposedServiceGraph(const std::filesystem::path& source_root,
                                                                    const std::filesystem::path& binary_root,
                                                                    const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path phase_doc_path = source_root / "docs" / "plan" / "active" /
                                                 "phase140_serial_ingress_composed_service_graph.txt";
    const std::filesystem::path position_path = source_root / "docs" / "plan" / "admin" /
                                                "modern_c_canopus_readiness_position.txt";
    const std::filesystem::path tool_readme_path = source_root / "tests" / "tool" / "README.md";
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase140_serial_ingress_composed_service_graph.mirproj.txt";
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
                                                                 build_dir / "kernel_phase140_serial_ingress_composed_service_graph_build_output.txt",
                                                                 "freestanding kernel phase140 serial-ingress composed service graph build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase140 freestanding kernel serial-ingress composed service graph build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase140BehaviorSlice(build_dir, build_targets);
    ExpectPhase140PublicationSlice(phase_doc_path,
                                   ResolveCanopusRoadmapPath(source_root, 140),
                                   position_path,
                                   common_paths.kernel_readme_path,
                                   common_paths.repo_map_path,
                                   tool_readme_path,
                                   common_paths.freestanding_readme_path,
                                   common_paths.decision_log_path,
                                   common_paths.backlog_path);
    ExpectPhase140MirStructureSlice(dump_targets.mir, mir_projection_path);
}

}  // namespace mc::tool_tests