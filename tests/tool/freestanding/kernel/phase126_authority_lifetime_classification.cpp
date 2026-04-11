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

void ExpectPhase126BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase126_lifetime_run_output.txt",
                                                             "freestanding kernel phase126 authority lifetime run");
    ExpectExitCodeAtLeast(run_outcome,
                          126,
                          run_output,
                          "phase126 freestanding kernel authority-lifetime run should preserve the landed phase126 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "kernel__bootstrap_audit.o")) {
        Fail("phase126 authority lifetime audit should emit the bootstrap_audit module object");
    }
}

void ExpectPhase126PublicationSlice(const std::filesystem::path& phase_doc_path,
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
                         "Phase 126 -- Authority Lifetime Classification",
                         "phase126 plan note should exist in canonical phase-doc style");
    ExpectOutputContains(phase_doc,
                         "Keep the compiler, sema, MIR, backend, ABI, and `hal` surfaces closed.",
                         "phase126 plan note should record the decision not to widen compiler work");
    ExpectOutputContains(phase_doc,
                         "Phase 126 is complete as an authority lifetime classification step.",
                         "phase126 plan note should close out the lifetime boundary explicitly");

    const std::string roadmap = ReadFile(roadmap_path);
    ExpectOutputContains(roadmap,
                         "Phase 126 is now concrete as one authority lifetime classification step",
                         "phase126 roadmap should record the landed lifetime boundary");

    const std::string position = ReadFile(position_path);
    ExpectOutputContains(position,
                         "landed Phase 126 authority lifetime classification step",
                         "phase126 position note should record the landed lifetime step");

    const std::string kernel_readme = ReadFile(kernel_readme_path);
    ExpectOutputContains(kernel_readme,
                         "bounded authority lifetime classification step",
                         "phase126 kernel README should describe the new lifetime boundary");

    const std::string repo_map = ReadFile(repo_map_path);
    ExpectOutputContains(repo_map,
                         "phase126_authority_lifetime_classification.cpp",
                         "phase126 repository map should list the new kernel proof owner");

    const std::string tool_readme = ReadFile(tool_readme_path);
    ExpectOutputContains(tool_readme,
                         "kernel/phase126_authority_lifetime_classification.cpp",
                         "phase126 tool README should list the new kernel proof owner");

    const std::string freestanding_readme = ReadFile(freestanding_readme_path);
    ExpectOutputContains(freestanding_readme,
                         "phase126_authority_lifetime_classification.cpp",
                         "phase126 freestanding README should list the new kernel proof owner");

    const std::string decision_log = ReadFile(decision_log_path);
    ExpectOutputContains(decision_log,
                         "Phase 126 authority lifetime classification",
                         "phase126 decision log should record the limited-scope lifetime decision");

    const std::string backlog = ReadFile(backlog_path);
    ExpectOutputContains(backlog,
                         "keep the Phase 126 authority lifetime classification explicit",
                         "phase126 backlog should keep the new lifetime boundary visible for later phases");
}

void ExpectPhase126MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "TypeDecl kind=struct name=debug.Phase126AuthorityLifetimeAudit",
            "Function name=bootstrap_audit.build_phase126_authority_lifetime_audit returns=[debug.Phase126AuthorityLifetimeAudit]",
            "Function name=debug.validate_phase126_authority_lifetime_classification returns=[bool]",
        },
        expected_projection_path,
        "phase126 merged MIR should preserve the authority lifetime projection");
}

}  // namespace

void RunFreestandingKernelPhase126AuthorityLifetimeClassification(const std::filesystem::path& source_root,
                                                                 const std::filesystem::path& binary_root,
                                                                 const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path phase_doc_path = ResolvePlanDocPath(source_root,
                                                                    "phase126_authority_lifetime_classification.txt");
    const std::filesystem::path position_path = source_root / "docs" / "plan" / "admin" /
                                                "modern_c_canopus_readiness_position.txt";
    const std::filesystem::path tool_readme_path = source_root / "tests" / "tool" / "README.md";
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase126_authority_lifetime_classification.mirproj.txt";
    const std::filesystem::path build_dir = binary_root / "kernel_phase126_authority_lifetime_build";
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
                                                                 build_dir / "kernel_phase126_authority_lifetime_build_output.txt",
                                                                 "freestanding kernel phase126 authority lifetime build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase126 freestanding kernel authority-lifetime build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase126BehaviorSlice(build_dir, build_targets);
    ExpectPhase126PublicationSlice(phase_doc_path,
                                   common_paths.roadmap_path,
                                   position_path,
                                   common_paths.kernel_readme_path,
                                   common_paths.repo_map_path,
                                   tool_readme_path,
                                   common_paths.freestanding_readme_path,
                                   common_paths.decision_log_path,
                                   common_paths.backlog_path);
    ExpectPhase126MirStructureSlice(dump_targets.mir, mir_projection_path);
}

}  // namespace mc::tool_tests