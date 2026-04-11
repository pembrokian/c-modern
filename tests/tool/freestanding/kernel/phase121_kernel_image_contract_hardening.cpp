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

void ExpectPhase121BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase121_image_contract_run_output.txt",
                                                             "freestanding kernel phase121 image-contract hardening run");
    ExpectExitCodeAtLeast(run_outcome,
                          121,
                          run_output,
                          "phase121 freestanding kernel image-contract hardening run should preserve the landed phase121 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_bootstrap_audit.mc.o")) {
        Fail("phase121 image-contract hardening audit should emit the bootstrap_audit module object");
    }
}

void ExpectPhase121PublicationSlice(const std::filesystem::path& phase_doc_path,
                                    const std::filesystem::path& support_note_path,
                                    const std::filesystem::path& roadmap_path,
                                    const std::filesystem::path& position_path,
                                    const std::filesystem::path& kernel_readme_path,
                                    const std::filesystem::path& repo_map_path,
                                    const std::filesystem::path& freestanding_readme_path,
                                    const std::filesystem::path& decision_log_path,
                                    const std::filesystem::path& backlog_path) {
    const std::string phase_doc = ReadFile(phase_doc_path);
    ExpectOutputContains(phase_doc,
                         "Phase 121 -- Kernel Image Contract Hardening",
                         "phase121 plan note should exist in canonical phase-doc style");
    ExpectOutputContains(phase_doc,
                         "Keep the compiler, sema, MIR, backend, ABI, and `hal` surfaces closed.",
                         "phase121 plan note should record the decision not to widen compiler work");
    ExpectOutputContains(phase_doc,
                         "Phase 121 is complete as a kernel image-contract hardening step.",
                         "phase121 plan note should close out the image-contract boundary explicitly");

    const std::string support_note = ReadFile(support_note_path);
    ExpectOutputContains(support_note,
                         "manifest, target identity, runtime startup, emitted image-input",
                         "phase121 support note should keep the image contract explicit");

    const std::string roadmap = ReadFile(roadmap_path);
    ExpectOutputContains(roadmap,
                         "Phase 121 is now concrete as one kernel image-contract hardening step",
                         "phase121 roadmap should record the landed image-contract boundary");

    const std::string position = ReadFile(position_path);
    ExpectOutputContains(position,
                         "landed Phase 121 kernel image-contract hardening step",
                         "phase121 position note should record the landed image-contract step");

    const std::string kernel_readme = ReadFile(kernel_readme_path);
    ExpectOutputContains(kernel_readme,
                         "bounded kernel image-contract hardening step",
                         "phase121 kernel README should describe the new image-contract boundary");

    const std::string repo_map = ReadFile(repo_map_path);
    ExpectOutputContains(repo_map,
                         "phase121_kernel_image_contract_hardening.cpp",
                         "phase121 repository map should list the new kernel proof owner");

    const std::string freestanding_readme = ReadFile(freestanding_readme_path);
    ExpectOutputContains(freestanding_readme,
                         "phase121_kernel_image_contract_hardening.cpp",
                         "phase121 freestanding README should list the new kernel proof owner");

    const std::string decision_log = ReadFile(decision_log_path);
    ExpectOutputContains(decision_log,
                         "Phase 121 kernel image-contract hardening",
                         "phase121 decision log should record the limited-scope image-contract decision");

    const std::string backlog = ReadFile(backlog_path);
    ExpectOutputContains(backlog,
                         "keep the Phase 121 kernel image-contract hardening explicit",
                         "phase121 backlog should keep the image-contract boundary visible for later phases");
}

void ExpectPhase121MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "TypeDecl kind=struct name=debug.Phase121KernelImageContractAudit",
            "Function name=bootstrap_audit.build_phase121_kernel_image_contract_audit returns=[debug.Phase121KernelImageContractAudit]",
            "Function name=debug.validate_phase121_kernel_image_contract_hardening returns=[bool]",
        },
        expected_projection_path,
        "phase121 merged MIR should preserve the image-contract projection");
}

}  // namespace

void RunFreestandingKernelPhase121KernelImageContractHardening(const std::filesystem::path& source_root,
                                                               const std::filesystem::path& binary_root,
                                                               const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path phase_doc_path = source_root / "docs" / "plan" /
                                                 "phase121_kernel_image_contract_hardening.txt";
    const std::filesystem::path support_note_path = source_root / "docs" / "plan" /
                                                    "canopus_running_system_support_statement.txt";
    const std::filesystem::path position_path = source_root / "docs" / "plan" / "admin" /
                                                "modern_c_canopus_readiness_position.txt";
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase121_kernel_image_contract_hardening.mirproj.txt";
    const std::filesystem::path build_dir = binary_root / "kernel_phase121_image_contract_build";
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
                                                                 build_dir / "kernel_phase121_image_contract_build_output.txt",
                                                                 "freestanding kernel phase121 image-contract hardening build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase121 freestanding kernel image-contract hardening build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase121BehaviorSlice(build_dir, build_targets);
    ExpectPhase121PublicationSlice(phase_doc_path,
                                   support_note_path,
                                   common_paths.roadmap_path,
                                   position_path,
                                   common_paths.kernel_readme_path,
                                   common_paths.repo_map_path,
                                   common_paths.freestanding_readme_path,
                                   common_paths.decision_log_path,
                                   common_paths.backlog_path);
    ExpectPhase121MirStructureSlice(dump_targets.mir, mir_projection_path);
}

}  // namespace mc::tool_tests
