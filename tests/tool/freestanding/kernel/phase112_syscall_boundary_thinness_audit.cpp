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

void RunFreestandingKernelPhase112SyscallBoundaryThinnessAudit(const std::filesystem::path& source_root,
                                                               const std::filesystem::path& binary_root,
                                                               const std::filesystem::path& mc_path) {
    const std::filesystem::path project_path = source_root / "kernel" / "build.toml";
    const std::filesystem::path main_source_path = source_root / "kernel" / "src/main.mc";
    const std::filesystem::path syscall_source_path = source_root / "kernel" / "src/syscall.mc";
    const std::filesystem::path capability_source_path = source_root / "kernel" / "src/capability.mc";
    const std::filesystem::path endpoint_source_path = source_root / "kernel" / "src/endpoint.mc";
    const std::filesystem::path address_space_source_path = source_root / "kernel" / "src/address_space.mc";
    const std::filesystem::path debug_source_path = source_root / "kernel" / "src/debug.mc";
    const std::filesystem::path phase_doc_path = source_root / "docs" / "plan" /
                                                 "phase112_syscall_boundary_thinness_audit.txt";
    const std::filesystem::path kernel_readme_path = source_root / "kernel" / "README.md";
    const std::filesystem::path repo_map_path = source_root / "docs" / "agent" / "prompts" / "repo_map.md";
    const std::filesystem::path freestanding_readme_path = source_root / "tests" / "tool" / "freestanding" / "README.md";
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
    const auto [run_outcome, run_output] = RunCommandCapture({build_targets.executable.generic_string()},
                                                             build_dir / "kernel_phase112_syscall_boundary_run_output.txt",
                                                             "freestanding kernel phase112 syscall-boundary audit run");
    if (!run_outcome.exited || run_outcome.exit_code != 113) {
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
                         "Phase 113 has moved the repository-owned kernel artifact beyond the landed",
                         "phase112 kernel README should record the syscall-boundary audit as current status");
    ExpectOutputContains(kernel_readme,
                         "thin observation state over capability,",
                         "phase112 kernel README should describe syscall as a thin owner");

    const std::string repo_map = ReadFile(repo_map_path);
    ExpectOutputContains(repo_map,
                         "currently a Phase 113 interrupt-entry-and-generic-dispatch-bounded kernel target",
                         "phase112 repository map should describe the current kernel boundary");
    ExpectOutputContains(repo_map,
                         "phase112_syscall_boundary_thinness_audit.cpp",
                         "phase112 repository map should list the new kernel proof owner");

    const std::string freestanding_readme = ReadFile(freestanding_readme_path);
    ExpectOutputContains(freestanding_readme,
                         "phase112_syscall_boundary_thinness_audit.cpp",
                         "phase112 freestanding README should list the new kernel proof owner");

    const std::string main_source = ReadFile(main_source_path);
    ExpectOutputContains(main_source,
                         "const PHASE113_MARKER: i32 = 113",
                         "phase112 main module should advance the current kernel marker");
    ExpectOutputContains(main_source,
                         "capability.validate_syscall_capability_boundary",
                         "phase112 main module should validate the capability-owned syscall boundary");
    ExpectOutputContains(main_source,
                         "endpoint.validate_syscall_ipc_boundary",
                         "phase112 main module should validate the endpoint-owned syscall boundary");
    ExpectOutputContains(main_source,
                         "address_space.validate_syscall_address_space_boundary",
                         "phase112 main module should validate the address-space-owned syscall boundary");
    ExpectOutputContains(main_source,
                         "debug.validate_phase112_syscall_boundary_thinness",
                         "phase112 main module should call the new debug-owned phase112 audit");

    const std::string syscall_source = ReadFile(syscall_source_path);
    ExpectOutputContains(syscall_source,
                         "capability.resolve_endpoint_handle",
                         "phase112 syscall module should route endpoint handle lookup through the capability owner");
    ExpectOutputContains(syscall_source,
                         "endpoint.build_runtime_message",
                         "phase112 syscall module should route runtime message construction through the endpoint owner");
    ExpectOutputContains(syscall_source,
                         "endpoint.enqueue_runtime_message",
                         "phase112 syscall module should route queue send through the endpoint owner");
    ExpectOutputContains(syscall_source,
                         "endpoint.receive_runtime_message",
                         "phase112 syscall module should route queue receive through the endpoint owner");
    ExpectOutputContains(syscall_source,
                         "address_space.build_child_bootstrap_context",
                         "phase112 syscall module should route child bootstrap construction through the address-space owner");

    const std::string capability_source = ReadFile(capability_source_path);
    ExpectOutputContains(capability_source,
                         "func resolve_endpoint_handle(table: HandleTable, slot_id: u32) EndpointHandleResolution",
                         "phase112 capability module should own endpoint handle lookup for syscall use");
    ExpectOutputContains(capability_source,
                         "func resolve_attached_transfer_handle(table: HandleTable, attached_handle_slot: u32, attached_handle_count: usize) AttachedTransferResolution",
                         "phase112 capability module should own attached-handle transfer resolution");
    ExpectOutputContains(capability_source,
                         "func install_received_endpoint_handle(table: HandleTable, receive_handle_slot: u32, attached_count: usize, attached_endpoint_id: u32, attached_rights: u32) ReceivedHandleInstall",
                         "phase112 capability module should own receive-side handle installation");
    ExpectOutputContains(capability_source,
                         "func validate_syscall_capability_boundary() bool",
                         "phase112 capability module should expose a focused owner-local validator");

    const std::string endpoint_source = ReadFile(endpoint_source_path);
    ExpectOutputContains(endpoint_source,
                         "func build_runtime_message(message_id: u32, source_pid: u32, endpoint_id: u32, payload_len: usize, payload: [4]u8, attached_count: usize, attached_endpoint_id: u32, attached_rights: u32, attached_source_handle_slot: u32) KernelMessage",
                         "phase112 endpoint module should own runtime message construction");
    ExpectOutputContains(endpoint_source,
                         "func enqueue_runtime_message(table: EndpointTable, endpoint_id: u32, message: KernelMessage) RuntimeSendResult",
                         "phase112 endpoint module should own runtime queue send");
    ExpectOutputContains(endpoint_source,
                         "func receive_runtime_message(table: EndpointTable, endpoint_id: u32) RuntimeReceiveResult",
                         "phase112 endpoint module should own runtime queue receive");
    ExpectOutputContains(endpoint_source,
                         "func validate_syscall_ipc_boundary() bool",
                         "phase112 endpoint module should expose a focused owner-local validator");

    const std::string address_space_source = ReadFile(address_space_source_path);
    ExpectOutputContains(address_space_source,
                         "func build_child_bootstrap_context(owner_pid: u32, child_tid: u32, child_asid: u32, root_page_table: usize, image_base: usize, image_size: usize, entry_pc: usize, stack_base: usize, stack_size: usize, stack_top: usize) SpawnBootstrap",
                         "phase112 address-space module should own child bootstrap construction");
    ExpectOutputContains(address_space_source,
                         "func validate_syscall_address_space_boundary() bool",
                         "phase112 address-space module should expose a focused owner-local validator");

    const std::string debug_source = ReadFile(debug_source_path);
    ExpectOutputContains(debug_source,
                         "func validate_phase112_syscall_boundary_thinness(audit: RunningKernelSliceAudit, scheduler_contract_hardened: u32, lifecycle_contract_hardened: u32, capability_contract_hardened: u32, ipc_contract_hardened: u32, address_space_contract_hardened: u32) bool",
                         "phase112 debug module should own the new syscall-boundary audit");

    const std::string kernel_mir = ReadFile(dump_targets.mir);
    ExpectOutputContains(kernel_mir,
                         "ConstGlobal names=[PHASE113_MARKER] type=i32",
                         "phase112 merged MIR should expose the current kernel marker");
    ExpectOutputContains(kernel_mir,
                         "Function name=capability.validate_syscall_capability_boundary returns=[bool]",
                         "phase112 merged MIR should expose the capability-owned validator");
    ExpectOutputContains(kernel_mir,
                         "Function name=endpoint.validate_syscall_ipc_boundary returns=[bool]",
                         "phase112 merged MIR should expose the endpoint-owned validator");
    ExpectOutputContains(kernel_mir,
                         "Function name=address_space.validate_syscall_address_space_boundary returns=[bool]",
                         "phase112 merged MIR should expose the address-space-owned validator");
    ExpectOutputContains(kernel_mir,
                         "Function name=debug.validate_phase112_syscall_boundary_thinness returns=[bool]",
                         "phase112 merged MIR should expose the new debug-owned phase112 audit");
}

}  // namespace mc::tool_tests