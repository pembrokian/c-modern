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

void ExpectPhase112BehaviorSlice(const std::filesystem::path& build_dir,
                                 const mc::support::BuildArtifactTargets& build_targets) {
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase112_syscall_boundary_run_output.txt",
                                                             "freestanding kernel phase112 syscall-boundary audit run");
    if (!run_outcome.exited || run_outcome.exit_code != 116) {
        Fail("phase112 freestanding kernel syscall-boundary audit run should exit with the current kernel proof marker:\n" +
             run_output);
    }

    const std::filesystem::path object_dir = build_targets.object.parent_path();
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_capability.mc.o")) {
        Fail("phase112 syscall boundary audit should emit the capability module object");
    }
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_endpoint.mc.o")) {
        Fail("phase112 syscall boundary audit should emit the endpoint module object");
    }
    if (!std::filesystem::exists(object_dir / "_Users_ro_dev_c_modern_kernel_src_address_space.mc.o")) {
        Fail("phase112 syscall boundary audit should emit the address-space module object");
    }
}

void ExpectPhase112PublicationSlice(const std::filesystem::path& phase_doc_path,
                                    const std::filesystem::path& kernel_readme_path,
                                    const std::filesystem::path& repo_map_path,
                                    const std::filesystem::path& freestanding_readme_path) {
    const std::string phase_doc = ReadFile(phase_doc_path);
    ExpectOutputContains(phase_doc,
                         "Phase 112 -- Syscall Boundary Thinness Audit",
                         "phase112 plan note should exist in canonical phase-doc style");
    ExpectOutputContains(phase_doc,
                         "Do not expand the compiler or language surface unless this kernel step",
                         "phase112 plan note should record the decision not to widen compiler work");
    ExpectOutputContains(phase_doc,
                         "Phase 112 is complete as a syscall boundary thinness audit.",
                         "phase112 plan note should close out the syscall-boundary audit explicitly");

    const std::string kernel_readme = ReadFile(kernel_readme_path);
    ExpectOutputContains(kernel_readme,
                         "Phase 116 has moved the repository-owned kernel artifact beyond the landed",
                         "phase112 kernel README should record the syscall-boundary audit as current status");
    ExpectOutputContains(kernel_readme,
                         "thin observation state over capability,",
                         "phase112 kernel README should describe syscall as a thin owner");

    const std::string repo_map = ReadFile(repo_map_path);
    ExpectOutputContains(repo_map,
                         "currently a Phase 116 MMU-activation-barrier-hardened kernel target",
                         "phase112 repository map should describe the current kernel boundary");
    ExpectOutputContains(repo_map,
                         "phase112_syscall_boundary_thinness_audit.cpp",
                         "phase112 repository map should list the new kernel proof owner");

    const std::string freestanding_readme = ReadFile(freestanding_readme_path);
    ExpectOutputContains(freestanding_readme,
                         "phase112_syscall_boundary_thinness_audit.cpp",
                         "phase112 freestanding README should list the new kernel proof owner");
}

void ExpectPhase112MirStructureSlice(const std::filesystem::path& mir_path,
                                     const std::filesystem::path& expected_projection_path) {
    const std::string kernel_mir = ReadFile(mir_path);
    ExpectMirFirstMatchProjectionFile(
        kernel_mir,
        {
            "ConstGlobal names=[PHASE116_MARKER] type=i32",
            "Function name=address_space.build_child_bootstrap_context returns=[address_space.SpawnBootstrap]",
            "Function name=address_space.validate_syscall_address_space_boundary returns=[bool]",
            "Function name=capability.resolve_endpoint_handle returns=[capability.EndpointHandleResolution]",
            "Function name=capability.resolve_attached_transfer_handle returns=[capability.AttachedTransferResolution]",
            "Function name=capability.install_received_endpoint_handle returns=[capability.ReceivedHandleInstall]",
            "Function name=capability.validate_syscall_capability_boundary returns=[bool]",
            "Function name=endpoint.build_runtime_message returns=[endpoint.KernelMessage]",
            "Function name=endpoint.enqueue_runtime_message returns=[endpoint.RuntimeSendResult]",
            "Function name=endpoint.receive_runtime_message returns=[endpoint.RuntimeReceiveResult]",
            "Function name=endpoint.validate_syscall_ipc_boundary returns=[bool]",
            "Function name=debug.validate_phase112_syscall_boundary_thinness returns=[bool]",
        },
        expected_projection_path,
        "phase112 merged MIR should preserve the syscall boundary projection");
}

}  // namespace

void RunFreestandingKernelPhase112SyscallBoundaryThinnessAudit(const std::filesystem::path& source_root,
                                                               const std::filesystem::path& binary_root,
                                                               const std::filesystem::path& mc_path) {
    const std::filesystem::path project_path = source_root / "kernel" / "build.toml";
    const std::filesystem::path main_source_path = source_root / "kernel" / "src/main.mc";
    const std::filesystem::path phase_doc_path = source_root / "docs" / "plan" /
                                                 "phase112_syscall_boundary_thinness_audit.txt";
    const std::filesystem::path kernel_readme_path = source_root / "kernel" / "README.md";
    const std::filesystem::path repo_map_path = source_root / "docs" / "agent" / "prompts" / "repo_map.md";
    const std::filesystem::path freestanding_readme_path = source_root / "tests" / "tool" / "freestanding" / "README.md";
    const std::filesystem::path mir_projection_path = source_root / "tests" / "tool" / "freestanding" / "kernel" /
                                                      "phase112_syscall_boundary_thinness_audit.mirproj.txt";
    const std::filesystem::path build_dir = binary_root / "kernel_phase112_syscall_boundary_build";
    std::filesystem::remove_all(build_dir);

    const auto [build_outcome, build_output] = RunCommandCapture({mc_path.generic_string(),
                                                                  "build",
                                                                  "--project",
                                                                  project_path.generic_string(),
                                                                  "--target",
                                                                  "kernel",
                                                                  "--build-dir",
                                                                  build_dir.generic_string(),
                                                                  "--dump-mir"},
                                                                 build_dir / "kernel_phase112_syscall_boundary_build_output.txt",
                                                                 "freestanding kernel phase112 syscall-boundary audit build");
    if (!build_outcome.exited || build_outcome.exit_code != 0) {
        Fail("phase112 freestanding kernel syscall-boundary audit build should succeed:\n" + build_output);
    }

    const auto build_targets = mc::support::ComputeBuildArtifactTargets(main_source_path, build_dir);
    const auto dump_targets = mc::support::ComputeDumpTargets(main_source_path, build_dir);
    ExpectPhase112BehaviorSlice(build_dir, build_targets);
    ExpectPhase112PublicationSlice(phase_doc_path, kernel_readme_path, repo_map_path, freestanding_readme_path);
    ExpectPhase112MirStructureSlice(dump_targets.mir, mir_projection_path);
}

}  // namespace mc::tool_tests