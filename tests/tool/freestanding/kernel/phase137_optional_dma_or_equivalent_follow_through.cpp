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

void ExpectPhase137BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase137_optional_dma_follow_through_run_output.txt",
                                                             "freestanding kernel phase137 optional dma-or-equivalent follow-through run");
    ExpectExitCodeAtLeast(run_outcome,
                          137,
                          run_output,
                          "phase137 freestanding kernel optional dma-or-equivalent follow-through run should preserve the landed phase137 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_uart.mc.o")) {
        Fail("phase137 optional dma-or-equivalent probe should emit the uart module object");
    }
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_interrupt.mc.o")) {
        Fail("phase137 optional dma-or-equivalent probe should emit the interrupt module object");
    }
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_serial_service.mc.o")) {
        Fail("phase137 optional dma-or-equivalent probe should emit the serial_service module object");
    }
}

void ExpectPhase137PublicationSlice(const std::filesystem::path& phase_doc_path,
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
                         "Phase 137 -- Optional DMA-Or-Equivalent Follow-Through",
                         "phase137 plan note should exist in canonical phase-doc style");
    ExpectOutputContains(phase_doc,
                         "Phase 137 is complete as one bounded completion-backed UART receive follow-through.",
                         "phase137 plan note should close out the bounded completion-backed step explicitly");
    ExpectOutputContains(phase_doc,
                         "one active completion-backed receive buffer",
                         "phase137 plan note should record the bounded single-buffer completion shape");
    ExpectOutputContains(phase_doc,
                         "Do not create `DmaManager`, `IoStack`, reusable descriptor frameworks",
                         "phase137 plan note should keep the no-framework rule explicit");

    const std::string roadmap = ReadFile(roadmap_path);
    ExpectOutputContains(roadmap,
                         "Phase 137 is now concrete as one bounded optional completion-backed follow-through",
                         "phase137 roadmap should record the landed completion-backed follow-through step");

    const std::string position = ReadFile(position_path);
    ExpectOutputContains(position,
                         "landed Phase 137 optional completion-backed follow-through",
                         "phase137 position note should record the landed completion-backed follow-through step");

    const std::string kernel_readme = ReadFile(kernel_readme_path);
    ExpectOutputContains(kernel_readme,
                         "bounded optional completion-backed UART receive follow-through",
                         "phase137 kernel README should describe the new completion-backed boundary");

    const std::string repo_map = ReadFile(repo_map_path);
    ExpectOutputContains(repo_map,
                         "phase137_optional_dma_or_equivalent_follow_through.cpp",
                         "phase137 repository map should list the new kernel proof owner");

    const std::string tool_readme = ReadFile(tool_readme_path);
    ExpectOutputContains(tool_readme,
                         "kernel/phase137_optional_dma_or_equivalent_follow_through.cpp",
                         "phase137 tool README should list the new kernel proof owner");

    const std::string freestanding_readme = ReadFile(freestanding_readme_path);
    ExpectOutputContains(freestanding_readme,
                         "phase137_optional_dma_or_equivalent_follow_through.cpp",
                         "phase137 freestanding README should list the new kernel proof owner");

    const std::string decision_log = ReadFile(decision_log_path);
    ExpectOutputContains(decision_log,
                         "Phase 137 optional completion-backed follow-through",
                         "phase137 decision log should record the limited-scope completion-backed decision");
    ExpectOutputContains(decision_log,
                         "one active completion-backed receive buffer plus one completion interrupt plus one explicit service consume-and-retire path",
                         "phase137 decision log should record the choice not to widen into DMA or descriptor frameworks");

    const std::string backlog = ReadFile(backlog_path);
    ExpectOutputContains(backlog,
                         "keep the Phase 137 optional completion-backed follow-through explicit",
                         "phase137 backlog should keep the completion-backed boundary visible for later phases");
}

void ExpectPhase137MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "TypeDecl kind=struct name=debug.Phase137OptionalDmaOrEquivalentAudit",
            "Function name=uart.handle_receive_completion_interrupt returns=[uart.UartCompletionInterruptResult]",
            "Function name=interrupt.dispatch_interrupt returns=[interrupt.InterruptDispatchResult]",
            "Function name=serial_service.record_ingress returns=[serial_service.SerialServiceState]",
            "Function name=debug.validate_phase137_optional_dma_or_equivalent_follow_through returns=[bool]",
        },
        expected_projection_path,
        "phase137 merged MIR should preserve the completion-backed boundary projection");
}

}  // namespace

void RunFreestandingKernelPhase137OptionalDmaOrEquivalentFollowThrough(const std::filesystem::path& source_root,
                                                                       const std::filesystem::path& binary_root,
                                                                       const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path phase_doc_path = source_root / "docs" / "plan" / "active" /
                                                 "phase137_optional_dma_or_equivalent_follow_through.txt";
    const std::filesystem::path position_path = source_root / "docs" / "plan" / "admin" /
                                                "modern_c_canopus_readiness_position.txt";
    const std::filesystem::path tool_readme_path = source_root / "tests" / "tool" / "README.md";
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase137_optional_dma_or_equivalent_follow_through.mirproj.txt";
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
                                                                 build_dir / "kernel_phase137_optional_dma_follow_through_build_output.txt",
                                                                 "freestanding kernel phase137 optional dma-or-equivalent follow-through build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase137 freestanding kernel optional dma-or-equivalent follow-through build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase137BehaviorSlice(build_dir, build_targets);
    ExpectPhase137PublicationSlice(phase_doc_path,
                                   ResolveCanopusRoadmapPath(source_root, 137),
                                   position_path,
                                   common_paths.kernel_readme_path,
                                   common_paths.repo_map_path,
                                   tool_readme_path,
                                   common_paths.freestanding_readme_path,
                                   common_paths.decision_log_path,
                                   common_paths.backlog_path);
    ExpectPhase137MirStructureSlice(dump_targets.mir, mir_projection_path);
}

}  // namespace mc::tool_tests
