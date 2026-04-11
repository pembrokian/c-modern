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

void ExpectPhase114BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase114_address_space_mmu_run_output.txt",
                                                             "freestanding kernel phase114 address-space/mmu run");
    ExpectExitCodeAtLeast(run_outcome,
                          114,
                          run_output,
                          "phase114 freestanding kernel address-space/mmu run should preserve the landed phase114 slice");

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_mmu.mc.o")) {
        Fail("phase114 address-space/mmu audit should emit the mmu module object");
    }
}

void ExpectPhase114MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "TypeDecl kind=struct name=mmu.TranslationRoot",
            "Function name=mmu.bootstrap_translation_root returns=[mmu.TranslationRoot]",
            "Function name=mmu.validate_address_space_mmu_boundary returns=[bool]",
            "Function name=debug.validate_phase114_address_space_and_mmu_ownership_split returns=[bool]",
        },
        expected_projection_path,
        "phase114 merged MIR should preserve the address-space/mmu boundary projection");
}

}  // namespace

void RunFreestandingKernelPhase114AddressSpaceAndMmuOwnershipSplit(const std::filesystem::path& source_root,
                                                                   const std::filesystem::path& binary_root,
                                                                   const std::filesystem::path& mc_path) {
    const auto common_paths = MakeFreestandingKernelCommonPaths(source_root);
    const std::filesystem::path build_dir = binary_root / "kernel_phase114_address_space_mmu_build";
    const std::filesystem::path phase_doc_path = ResolvePlanDocPath(source_root,
                                                                    "phase114_address_space_and_mmu_ownership_split.txt");
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase114_address_space_and_mmu_ownership_split.mirproj.txt";
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
                                                                 build_dir / "kernel_phase114_address_space_mmu_build_output.txt",
                                                                 "freestanding kernel phase114 address-space/mmu build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase114 freestanding kernel address-space/mmu build should succeed:\n" + build_output);
    }

    const std::string phase_doc = ReadFile(phase_doc_path);
    ExpectOutputContains(phase_doc,
                         "Phase 114 -- Address-Space And MMU Ownership Split",
                         "phase114 plan note should exist in canonical phase-doc style");

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(common_paths.main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(common_paths.main_source_path, build_dir);
    ExpectPhase114BehaviorSlice(build_dir, build_targets);
    ExpectPhase114MirStructureSlice(dump_targets.mir, mir_projection_path);
}

}  // namespace mc::tool_tests