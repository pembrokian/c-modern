import address_space
import capability
import echo_service
import init
import interrupt
import log_service
import state
import syscall
import timer
import transfer_service

struct Phase108ProgramCapContract {
    init_pid: u32
    log_service_program_object_id: u32
    echo_service_program_object_id: u32
    transfer_service_program_object_id: u32
    log_service_wait_handle_slot: u32
    echo_service_wait_handle_slot: u32
    transfer_service_wait_handle_slot: u32
    log_service_exit_code: i32
    echo_service_exit_code: i32
    transfer_service_exit_code: i32
    bootstrap_program_capability: capability.CapabilitySlot
    log_service_program_capability: capability.CapabilitySlot
    echo_service_program_capability: capability.CapabilitySlot
    transfer_service_program_capability: capability.CapabilitySlot
    log_service_spawn: syscall.SpawnObservation
    echo_service_spawn: syscall.SpawnObservation
    transfer_service_spawn: syscall.SpawnObservation
    log_service_wait: syscall.WaitObservation
    echo_service_wait: syscall.WaitObservation
    transfer_service_wait: syscall.WaitObservation
}

struct RunningKernelSliceAudit {
    kernel: state.KernelDescriptor
    init_pid: u32
    init_tid: u32
    init_asid: u32
    child_tid: u32
    child_exit_code: i32
    transfer_endpoint_id: u32
    log_service_request_byte: u8
    echo_service_request_byte0: u8
    echo_service_request_byte1: u8
    log_service_exit_code: i32
    echo_service_exit_code: i32
    transfer_service_exit_code: i32
    init_bootstrap_handoff: init.BootstrapHandoffObservation
    receive_observation: syscall.ReceiveObservation
    attached_receive_observation: syscall.ReceiveObservation
    transferred_handle_use_observation: syscall.ReceiveObservation
    pre_exit_wait_observation: syscall.WaitObservation
    exit_wait_observation: syscall.WaitObservation
    sleep_observation: syscall.SleepObservation
    timer_wake_observation: timer.TimerWakeObservation
    log_service_handshake: log_service.LogHandshakeObservation
    log_service_wait_observation: syscall.WaitObservation
    echo_service_exchange: echo_service.EchoExchangeObservation
    echo_service_wait_observation: syscall.WaitObservation
    transfer_service_transfer: transfer_service.TransferObservation
    transfer_service_wait_observation: syscall.WaitObservation
    phase104_contract_hardened: u32
    phase108_contract_hardened: u32
    init_process: state.ProcessSlot
    init_task: state.TaskSlot
    init_user_frame: address_space.UserEntryFrame
    boot_log_append_failed: u32
}

struct Phase117MultiServiceBringUpAudit {
    running_slice: RunningKernelSliceAudit
    init_endpoint_id: u32
    transfer_endpoint_id: u32
    log_service_program_capability: capability.CapabilitySlot
    echo_service_program_capability: capability.CapabilitySlot
    transfer_service_program_capability: capability.CapabilitySlot
    log_service_spawn: syscall.SpawnObservation
    echo_service_spawn: syscall.SpawnObservation
    transfer_service_spawn: syscall.SpawnObservation
    log_service_wait: syscall.WaitObservation
    echo_service_wait: syscall.WaitObservation
    transfer_service_wait: syscall.WaitObservation
    log_service_handshake: log_service.LogHandshakeObservation
    echo_service_exchange: echo_service.EchoExchangeObservation
    transfer_service_transfer: transfer_service.TransferObservation
}

struct Phase118DelegatedRequestReplyAudit {
    phase117: Phase117MultiServiceBringUpAudit
    transfer_service_transfer: transfer_service.TransferObservation
    invalidated_source_send_status: syscall.SyscallStatus
    invalidated_source_handle_slot: u32
    retained_receive_handle_slot: u32
    retained_receive_endpoint_id: u32
}

struct Phase119NamespacePressureAudit {
    phase118: Phase118DelegatedRequestReplyAudit
    directory_owner_pid: u32
    directory_entry_count: usize
    log_service_key: u32
    echo_service_key: u32
    transfer_service_key: u32
    shared_directory_endpoint_id: u32
    log_service_program_slot: u32
    echo_service_program_slot: u32
    transfer_service_program_slot: u32
    log_service_program_object_id: u32
    echo_service_program_object_id: u32
    transfer_service_program_object_id: u32
    log_service_wait_handle_slot: u32
    echo_service_wait_handle_slot: u32
    transfer_service_wait_handle_slot: u32
    dynamic_namespace_visible: u32
}

struct Phase120RunningSystemSupportAudit {
    phase119: Phase119NamespacePressureAudit
    service_policy_owner_pid: u32
    running_service_count: usize
    fixed_directory_count: usize
    shared_control_endpoint_id: u32
    retained_reply_endpoint_id: u32
    program_capability_count: usize
    wait_handle_count: usize
    dynamic_loading_visible: u32
    service_manager_visible: u32
    dynamic_namespace_visible: u32
}

struct Phase121KernelImageContractAudit {
    phase120: Phase120RunningSystemSupportAudit
    kernel_manifest_visible: u32
    kernel_target_visible: u32
    kernel_runtime_startup_visible: u32
    bootstrap_target_family_visible: u32
    emitted_image_input_visible: u32
    linked_kernel_executable_visible: u32
    dynamic_loading_visible: u32
    service_manager_visible: u32
    dynamic_namespace_visible: u32
}

struct Phase122TargetSurfaceAudit {
    phase121: Phase121KernelImageContractAudit
    kernel_target_visible: u32
    kernel_runtime_startup_visible: u32
    bootstrap_target_family_visible: u32
    bootstrap_target_family_only_visible: u32
    broader_target_family_visible: u32
    dynamic_loading_visible: u32
    service_manager_visible: u32
    dynamic_namespace_visible: u32
}

struct Phase123NextPlateauAudit {
    phase122: Phase122TargetSurfaceAudit
    running_kernel_truth_visible: u32
    running_system_truth_visible: u32
    kernel_image_truth_visible: u32
    target_surface_truth_visible: u32
    broader_platform_visible: u32
    broad_target_support_visible: u32
    general_loading_visible: u32
    compiler_reopening_visible: u32
}

struct Phase124DelegationChainAudit {
    phase123: Phase123NextPlateauAudit
    delegator_pid: u32
    intermediary_pid: u32
    final_holder_pid: u32
    control_endpoint_id: u32
    delegated_endpoint_id: u32
    delegator_source_handle_slot: u32
    intermediary_receive_handle_slot: u32
    final_receive_handle_slot: u32
    first_invalidated_send_status: syscall.SyscallStatus
    second_invalidated_send_status: syscall.SyscallStatus
    final_send_status: syscall.SyscallStatus
    final_send_source_pid: u32
    final_endpoint_queue_depth: usize
    ambient_authority_visible: u32
    compiler_reopening_visible: u32
}

struct Phase125InvalidationAudit {
    phase124: Phase124DelegationChainAudit
    invalidated_holder_pid: u32
    control_endpoint_id: u32
    invalidated_endpoint_id: u32
    invalidated_handle_slot: u32
    rejected_send_status: syscall.SyscallStatus
    rejected_receive_status: syscall.SyscallStatus
    surviving_control_send_status: syscall.SyscallStatus
    surviving_control_source_pid: u32
    surviving_control_queue_depth: usize
    authority_loss_visible: u32
    broader_revocation_visible: u32
    compiler_reopening_visible: u32
}

struct Phase126AuthorityLifetimeAudit {
    phase125: Phase125InvalidationAudit
    classified_holder_pid: u32
    long_lived_endpoint_id: u32
    short_lived_endpoint_id: u32
    long_lived_handle_slot: u32
    short_lived_handle_slot: u32
    repeat_long_lived_send_status: syscall.SyscallStatus
    repeat_long_lived_source_pid: u32
    repeat_long_lived_queue_depth: usize
    long_lived_class_visible: u32
    short_lived_class_visible: u32
    broader_lifetime_framework_visible: u32
    compiler_reopening_visible: u32
}

func validate_phase108_kernel_image_and_program_cap_contracts(contract: Phase108ProgramCapContract) bool {
    if !capability.is_program_capability(contract.bootstrap_program_capability) {
        return false
    }
    if contract.bootstrap_program_capability.owner_pid != contract.init_pid {
        return false
    }
    if contract.bootstrap_program_capability.slot_id != 2 {
        return false
    }
    if contract.bootstrap_program_capability.rights != 7 {
        return false
    }
    if contract.bootstrap_program_capability.object_id != 1 {
        return false
    }
    if contract.bootstrap_program_capability.object_id == contract.log_service_program_object_id {
        return false
    }
    if contract.bootstrap_program_capability.object_id == contract.echo_service_program_object_id {
        return false
    }
    if contract.bootstrap_program_capability.object_id == contract.transfer_service_program_object_id {
        return false
    }
    if capability.kind_score(contract.log_service_program_capability.kind) != 1 {
        return false
    }
    if capability.kind_score(contract.echo_service_program_capability.kind) != 1 {
        return false
    }
    if capability.kind_score(contract.transfer_service_program_capability.kind) != 1 {
        return false
    }
    if contract.log_service_spawn.wait_handle_slot != contract.log_service_wait_handle_slot {
        return false
    }
    if contract.echo_service_spawn.wait_handle_slot != contract.echo_service_wait_handle_slot {
        return false
    }
    if contract.transfer_service_spawn.wait_handle_slot != contract.transfer_service_wait_handle_slot {
        return false
    }
    if contract.log_service_wait.exit_code != contract.log_service_exit_code {
        return false
    }
    if contract.echo_service_wait.exit_code != contract.echo_service_exit_code {
        return false
    }
    if contract.transfer_service_wait.exit_code != contract.transfer_service_exit_code {
        return false
    }
    return true
}

func validate_phase109_first_running_kernel_slice(audit: RunningKernelSliceAudit) bool {
    if audit.kernel.booted != 1 {
        return false
    }
    if audit.kernel.current_pid != audit.init_pid {
        return false
    }
    if audit.kernel.current_tid != audit.init_tid {
        return false
    }
    if audit.kernel.active_asid != audit.init_asid {
        return false
    }
    if audit.kernel.user_entry_started != 1 {
        return false
    }
    if audit.init_bootstrap_handoff.authority_count != 2 {
        return false
    }
    if audit.init_bootstrap_handoff.ambient_root_visible != 0 {
        return false
    }
    if syscall.status_score(audit.receive_observation.status) != 2 {
        return false
    }
    if audit.receive_observation.payload_len != 4 {
        return false
    }
    if audit.receive_observation.payload[0] != 83 {
        return false
    }
    if audit.receive_observation.payload[1] != 89 {
        return false
    }
    if audit.receive_observation.payload[2] != 83 {
        return false
    }
    if audit.receive_observation.payload[3] != 67 {
        return false
    }
    if syscall.status_score(audit.attached_receive_observation.status) != 2 {
        return false
    }
    if audit.attached_receive_observation.received_handle_count != 1 {
        return false
    }
    if syscall.status_score(audit.transferred_handle_use_observation.status) != 2 {
        return false
    }
    if audit.transferred_handle_use_observation.payload_len != 4 {
        return false
    }
    if audit.transferred_handle_use_observation.payload[0] != 77 {
        return false
    }
    if audit.transferred_handle_use_observation.payload[1] != 79 {
        return false
    }
    if audit.transferred_handle_use_observation.payload[2] != 86 {
        return false
    }
    if audit.transferred_handle_use_observation.payload[3] != 69 {
        return false
    }
    if syscall.status_score(audit.pre_exit_wait_observation.status) != 4 {
        return false
    }
    if syscall.status_score(audit.exit_wait_observation.status) != 2 {
        return false
    }
    if audit.exit_wait_observation.exit_code != audit.child_exit_code {
        return false
    }
    if syscall.status_score(audit.sleep_observation.status) != 4 {
        return false
    }
    if syscall.block_reason_score(audit.sleep_observation.block_reason) != 16 {
        return false
    }
    if audit.timer_wake_observation.task_id != audit.child_tid {
        return false
    }
    if audit.timer_wake_observation.wake_tick != 1 {
        return false
    }
    if audit.phase104_contract_hardened == 0 {
        return false
    }
    if audit.log_service_handshake.request_count != 1 {
        return false
    }
    if audit.log_service_handshake.ack_count != 1 {
        return false
    }
    if audit.log_service_handshake.request_byte != audit.log_service_request_byte {
        return false
    }
    if audit.log_service_handshake.ack_byte != 33 {
        return false
    }
    if audit.log_service_wait_observation.exit_code != audit.log_service_exit_code {
        return false
    }
    if audit.echo_service_exchange.reply_count != 1 {
        return false
    }
    if audit.echo_service_exchange.reply_byte0 != audit.echo_service_request_byte0 {
        return false
    }
    if audit.echo_service_exchange.reply_byte1 != audit.echo_service_request_byte1 {
        return false
    }
    if audit.echo_service_wait_observation.exit_code != audit.echo_service_exit_code {
        return false
    }
    if audit.transfer_service_transfer.transferred_endpoint_id != audit.transfer_endpoint_id {
        return false
    }
    if audit.transfer_service_transfer.transferred_rights != 5 {
        return false
    }
    if audit.transfer_service_transfer.emit_count != 1 {
        return false
    }
    if audit.transfer_service_wait_observation.exit_code != audit.transfer_service_exit_code {
        return false
    }
    if audit.phase108_contract_hardened == 0 {
        return false
    }
    if audit.init_process.pid != audit.init_pid {
        return false
    }
    if audit.init_task.tid != audit.init_tid {
        return false
    }
    if audit.init_user_frame.task_id != audit.init_tid {
        return false
    }
    if audit.boot_log_append_failed != 0 {
        return false
    }
    return true
}

func validate_phase110_kernel_ownership_split(audit: RunningKernelSliceAudit, scheduler_contract_hardened: u32) bool {
    if scheduler_contract_hardened == 0 {
        return false
    }
    if audit.phase104_contract_hardened == 0 {
        return false
    }
    if audit.phase108_contract_hardened == 0 {
        return false
    }
    return validate_phase109_first_running_kernel_slice(audit)
}

func validate_phase111_scheduler_and_lifecycle_ownership(audit: RunningKernelSliceAudit, scheduler_contract_hardened: u32, lifecycle_contract_hardened: u32) bool {
    if lifecycle_contract_hardened == 0 {
        return false
    }
    return validate_phase110_kernel_ownership_split(audit, scheduler_contract_hardened)
}

func validate_phase112_syscall_boundary_thinness(audit: RunningKernelSliceAudit, scheduler_contract_hardened: u32, lifecycle_contract_hardened: u32, capability_contract_hardened: u32, ipc_contract_hardened: u32, address_space_contract_hardened: u32) bool {
    if capability_contract_hardened == 0 {
        return false
    }
    if ipc_contract_hardened == 0 {
        return false
    }
    if address_space_contract_hardened == 0 {
        return false
    }
    return validate_phase111_scheduler_and_lifecycle_ownership(audit, scheduler_contract_hardened, lifecycle_contract_hardened)
}

func validate_phase113_interrupt_entry_and_generic_dispatch_boundary(audit: RunningKernelSliceAudit, scheduler_contract_hardened: u32, lifecycle_contract_hardened: u32, capability_contract_hardened: u32, ipc_contract_hardened: u32, address_space_contract_hardened: u32, interrupt_contract_hardened: u32, interrupt_dispatch_kind: interrupt.InterruptDispatchKind) bool {
    if interrupt_contract_hardened == 0 {
        return false
    }
    if interrupt.dispatch_kind_score(interrupt_dispatch_kind) != 2 {
        return false
    }
    return validate_phase112_syscall_boundary_thinness(audit, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened)
}

func validate_phase114_address_space_and_mmu_ownership_split(audit: RunningKernelSliceAudit, scheduler_contract_hardened: u32, lifecycle_contract_hardened: u32, capability_contract_hardened: u32, ipc_contract_hardened: u32, address_space_contract_hardened: u32, interrupt_contract_hardened: u32) bool {
    if interrupt_contract_hardened == 0 {
        return false
    }
    if audit.kernel.active_asid != audit.init_asid {
        return false
    }
    if audit.init_user_frame.address_space_id != audit.init_asid {
        return false
    }
    return validate_phase113_interrupt_entry_and_generic_dispatch_boundary(audit, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, interrupt.InterruptDispatchKind.TimerWake)
}

func validate_phase115_timer_ownership_hardening(audit: RunningKernelSliceAudit, scheduler_contract_hardened: u32, lifecycle_contract_hardened: u32, capability_contract_hardened: u32, ipc_contract_hardened: u32, address_space_contract_hardened: u32, interrupt_contract_hardened: u32, timer_contract_hardened: u32) bool {
    if timer_contract_hardened == 0 {
        return false
    }
    if audit.timer_wake_observation.task_id != audit.child_tid {
        return false
    }
    if audit.timer_wake_observation.wake_count != 1 {
        return false
    }
    return validate_phase114_address_space_and_mmu_ownership_split(audit, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened)
}

func validate_phase116_mmu_activation_barrier_follow_through(audit: RunningKernelSliceAudit, scheduler_contract_hardened: u32, lifecycle_contract_hardened: u32, capability_contract_hardened: u32, ipc_contract_hardened: u32, address_space_contract_hardened: u32, interrupt_contract_hardened: u32, timer_contract_hardened: u32, barrier_contract_hardened: u32) bool {
    if barrier_contract_hardened == 0 {
        return false
    }
    if audit.kernel.active_asid != audit.init_asid {
        return false
    }
    return validate_phase115_timer_ownership_hardening(audit, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened)
}

func validate_phase117_init_orchestrated_multi_service_bring_up(audit: Phase117MultiServiceBringUpAudit, scheduler_contract_hardened: u32, lifecycle_contract_hardened: u32, capability_contract_hardened: u32, ipc_contract_hardened: u32, address_space_contract_hardened: u32, interrupt_contract_hardened: u32, timer_contract_hardened: u32, barrier_contract_hardened: u32) bool {
    if !validate_phase116_mmu_activation_barrier_follow_through(audit.running_slice, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened, barrier_contract_hardened) {
        return false
    }
    if audit.log_service_handshake.client_pid != audit.running_slice.init_pid {
        return false
    }
    if audit.echo_service_exchange.client_pid != audit.running_slice.init_pid {
        return false
    }
    if audit.transfer_service_transfer.client_pid != audit.running_slice.init_pid {
        return false
    }
    if audit.log_service_handshake.endpoint_id != audit.init_endpoint_id {
        return false
    }
    if audit.echo_service_exchange.endpoint_id != audit.init_endpoint_id {
        return false
    }
    if audit.transfer_service_transfer.control_endpoint_id != audit.init_endpoint_id {
        return false
    }
    if audit.transfer_service_transfer.transferred_endpoint_id != audit.transfer_endpoint_id {
        return false
    }
    return true
}

func validate_phase118_request_reply_and_delegation_follow_through(audit: Phase118DelegatedRequestReplyAudit, scheduler_contract_hardened: u32, lifecycle_contract_hardened: u32, capability_contract_hardened: u32, ipc_contract_hardened: u32, address_space_contract_hardened: u32, interrupt_contract_hardened: u32, timer_contract_hardened: u32, barrier_contract_hardened: u32) bool {
    if !validate_phase117_init_orchestrated_multi_service_bring_up(audit.phase117, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened, barrier_contract_hardened) {
        return false
    }
    if audit.transfer_service_transfer.grant_count != 1 {
        return false
    }
    if audit.transfer_service_transfer.emit_count != 1 {
        return false
    }
    if audit.transfer_service_transfer.grant_len != audit.transfer_service_transfer.emit_len {
        return false
    }
    if audit.transfer_service_transfer.grant_byte0 != audit.transfer_service_transfer.emit_byte0 {
        return false
    }
    if audit.transfer_service_transfer.grant_byte1 != audit.transfer_service_transfer.emit_byte1 {
        return false
    }
    if audit.transfer_service_transfer.grant_byte2 != audit.transfer_service_transfer.emit_byte2 {
        return false
    }
    if audit.transfer_service_transfer.grant_byte3 != audit.transfer_service_transfer.emit_byte3 {
        return false
    }
    if syscall.status_score(audit.invalidated_source_send_status) != 8 {
        return false
    }
    if audit.invalidated_source_handle_slot == 0 {
        return false
    }
    if audit.retained_receive_handle_slot == 0 {
        return false
    }
    if audit.invalidated_source_handle_slot == audit.retained_receive_handle_slot {
        return false
    }
    if audit.retained_receive_endpoint_id != audit.phase117.transfer_endpoint_id {
        return false
    }
    return true
}

func validate_phase119_namespace_pressure_audit(audit: Phase119NamespacePressureAudit, scheduler_contract_hardened: u32, lifecycle_contract_hardened: u32, capability_contract_hardened: u32, ipc_contract_hardened: u32, address_space_contract_hardened: u32, interrupt_contract_hardened: u32, timer_contract_hardened: u32, barrier_contract_hardened: u32) bool {
    if !validate_phase118_request_reply_and_delegation_follow_through(audit.phase118, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened, barrier_contract_hardened) {
        return false
    }
    if audit.directory_owner_pid != audit.phase118.phase117.running_slice.init_pid {
        return false
    }
    if audit.directory_entry_count != 3 {
        return false
    }
    if audit.log_service_key != 1 {
        return false
    }
    if audit.echo_service_key != 2 {
        return false
    }
    if audit.transfer_service_key != 3 {
        return false
    }
    if audit.log_service_key == audit.echo_service_key {
        return false
    }
    if audit.log_service_key == audit.transfer_service_key {
        return false
    }
    if audit.echo_service_key == audit.transfer_service_key {
        return false
    }
    if audit.shared_directory_endpoint_id != audit.phase118.phase117.init_endpoint_id {
        return false
    }
    if audit.shared_directory_endpoint_id == audit.phase118.phase117.transfer_endpoint_id {
        return false
    }
    if audit.log_service_program_slot == 0 {
        return false
    }
    if audit.echo_service_program_slot == 0 {
        return false
    }
    if audit.transfer_service_program_slot == 0 {
        return false
    }
    if audit.log_service_program_slot == audit.echo_service_program_slot {
        return false
    }
    if audit.log_service_program_slot == audit.transfer_service_program_slot {
        return false
    }
    if audit.echo_service_program_slot == audit.transfer_service_program_slot {
        return false
    }
    if audit.log_service_program_object_id == 0 {
        return false
    }
    if audit.echo_service_program_object_id == 0 {
        return false
    }
    if audit.transfer_service_program_object_id == 0 {
        return false
    }
    if audit.log_service_program_object_id == audit.echo_service_program_object_id {
        return false
    }
    if audit.log_service_program_object_id == audit.transfer_service_program_object_id {
        return false
    }
    if audit.echo_service_program_object_id == audit.transfer_service_program_object_id {
        return false
    }
    if audit.log_service_wait_handle_slot != audit.phase118.phase117.log_service_spawn.wait_handle_slot {
        return false
    }
    if audit.echo_service_wait_handle_slot != audit.phase118.phase117.echo_service_spawn.wait_handle_slot {
        return false
    }
    if audit.transfer_service_wait_handle_slot != audit.phase118.phase117.transfer_service_spawn.wait_handle_slot {
        return false
    }
    if audit.phase118.phase117.log_service_handshake.endpoint_id != audit.shared_directory_endpoint_id {
        return false
    }
    if audit.phase118.phase117.echo_service_exchange.endpoint_id != audit.shared_directory_endpoint_id {
        return false
    }
    if audit.phase118.phase117.transfer_service_transfer.control_endpoint_id != audit.shared_directory_endpoint_id {
        return false
    }
    return audit.dynamic_namespace_visible == 0
}

func validate_phase120_running_system_support_statement(audit: Phase120RunningSystemSupportAudit, scheduler_contract_hardened: u32, lifecycle_contract_hardened: u32, capability_contract_hardened: u32, ipc_contract_hardened: u32, address_space_contract_hardened: u32, interrupt_contract_hardened: u32, timer_contract_hardened: u32, barrier_contract_hardened: u32) bool {
    if !validate_phase119_namespace_pressure_audit(audit.phase119, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened, barrier_contract_hardened) {
        return false
    }
    if audit.service_policy_owner_pid != audit.phase119.directory_owner_pid {
        return false
    }
    if audit.running_service_count != audit.phase119.directory_entry_count {
        return false
    }
    if audit.running_service_count != 3 {
        return false
    }
    if audit.fixed_directory_count != 1 {
        return false
    }
    if audit.shared_control_endpoint_id != audit.phase119.shared_directory_endpoint_id {
        return false
    }
    if audit.retained_reply_endpoint_id != audit.phase119.phase118.retained_receive_endpoint_id {
        return false
    }
    if audit.retained_reply_endpoint_id != audit.phase119.phase118.phase117.transfer_endpoint_id {
        return false
    }
    if audit.program_capability_count != 3 {
        return false
    }
    if audit.wait_handle_count != 3 {
        return false
    }
    if audit.dynamic_loading_visible != 0 {
        return false
    }
    if audit.service_manager_visible != 0 {
        return false
    }
    return audit.dynamic_namespace_visible == 0
}

func validate_phase121_kernel_image_contract_hardening(audit: Phase121KernelImageContractAudit, scheduler_contract_hardened: u32, lifecycle_contract_hardened: u32, capability_contract_hardened: u32, ipc_contract_hardened: u32, address_space_contract_hardened: u32, interrupt_contract_hardened: u32, timer_contract_hardened: u32, barrier_contract_hardened: u32) bool {
    if !validate_phase120_running_system_support_statement(audit.phase120, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened, barrier_contract_hardened) {
        return false
    }
    if audit.kernel_manifest_visible != 1 {
        return false
    }
    if audit.kernel_target_visible != 1 {
        return false
    }
    if audit.kernel_runtime_startup_visible != 1 {
        return false
    }
    if audit.bootstrap_target_family_visible != 1 {
        return false
    }
    if audit.emitted_image_input_visible != 1 {
        return false
    }
    if audit.linked_kernel_executable_visible != 1 {
        return false
    }
    if audit.dynamic_loading_visible != 0 {
        return false
    }
    if audit.service_manager_visible != 0 {
        return false
    }
    return audit.dynamic_namespace_visible == 0
}

func validate_phase122_target_surface_audit(audit: Phase122TargetSurfaceAudit, scheduler_contract_hardened: u32, lifecycle_contract_hardened: u32, capability_contract_hardened: u32, ipc_contract_hardened: u32, address_space_contract_hardened: u32, interrupt_contract_hardened: u32, timer_contract_hardened: u32, barrier_contract_hardened: u32) bool {
    if !validate_phase121_kernel_image_contract_hardening(audit.phase121, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened, barrier_contract_hardened) {
        return false
    }
    if audit.kernel_target_visible != 1 {
        return false
    }
    if audit.kernel_runtime_startup_visible != 1 {
        return false
    }
    if audit.bootstrap_target_family_visible != 1 {
        return false
    }
    if audit.bootstrap_target_family_only_visible != 1 {
        return false
    }
    if audit.broader_target_family_visible != 0 {
        return false
    }
    if audit.dynamic_loading_visible != 0 {
        return false
    }
    if audit.service_manager_visible != 0 {
        return false
    }
    return audit.dynamic_namespace_visible == 0
}

func validate_phase123_next_plateau_audit(audit: Phase123NextPlateauAudit, scheduler_contract_hardened: u32, lifecycle_contract_hardened: u32, capability_contract_hardened: u32, ipc_contract_hardened: u32, address_space_contract_hardened: u32, interrupt_contract_hardened: u32, timer_contract_hardened: u32, barrier_contract_hardened: u32) bool {
    if !validate_phase122_target_surface_audit(audit.phase122, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened, barrier_contract_hardened) {
        return false
    }
    if audit.running_kernel_truth_visible != 1 {
        return false
    }
    if audit.running_system_truth_visible != 1 {
        return false
    }
    if audit.kernel_image_truth_visible != 1 {
        return false
    }
    if audit.target_surface_truth_visible != 1 {
        return false
    }
    if audit.broader_platform_visible != 0 {
        return false
    }
    if audit.broad_target_support_visible != 0 {
        return false
    }
    if audit.general_loading_visible != 0 {
        return false
    }
    return audit.compiler_reopening_visible == 0
}

func validate_phase124_delegation_chain_stress(audit: Phase124DelegationChainAudit, scheduler_contract_hardened: u32, lifecycle_contract_hardened: u32, capability_contract_hardened: u32, ipc_contract_hardened: u32, address_space_contract_hardened: u32, interrupt_contract_hardened: u32, timer_contract_hardened: u32, barrier_contract_hardened: u32) bool {
    if !validate_phase123_next_plateau_audit(audit.phase123, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened, barrier_contract_hardened) {
        return false
    }
    if audit.delegator_pid != audit.phase123.phase122.phase121.phase120.phase119.phase118.phase117.running_slice.init_pid {
        return false
    }
    if audit.intermediary_pid == 0 {
        return false
    }
    if audit.final_holder_pid == 0 {
        return false
    }
    if audit.intermediary_pid == audit.delegator_pid {
        return false
    }
    if audit.final_holder_pid == audit.delegator_pid {
        return false
    }
    if audit.intermediary_pid == audit.final_holder_pid {
        return false
    }
    if audit.control_endpoint_id != audit.phase123.phase122.phase121.phase120.phase119.phase118.phase117.init_endpoint_id {
        return false
    }
    if audit.delegated_endpoint_id != audit.phase123.phase122.phase121.phase120.phase119.phase118.phase117.transfer_endpoint_id {
        return false
    }
    if audit.control_endpoint_id == audit.delegated_endpoint_id {
        return false
    }
    if audit.delegator_source_handle_slot == 0 {
        return false
    }
    if audit.intermediary_receive_handle_slot == 0 {
        return false
    }
    if audit.final_receive_handle_slot == 0 {
        return false
    }
    if audit.delegator_source_handle_slot == audit.intermediary_receive_handle_slot {
        return false
    }
    if audit.intermediary_receive_handle_slot == audit.final_receive_handle_slot {
        return false
    }
    if syscall.status_score(audit.first_invalidated_send_status) != 8 {
        return false
    }
    if syscall.status_score(audit.second_invalidated_send_status) != 8 {
        return false
    }
    if syscall.status_score(audit.final_send_status) != 2 {
        return false
    }
    if audit.final_send_source_pid != audit.final_holder_pid {
        return false
    }
    if audit.final_endpoint_queue_depth != 1 {
        return false
    }
    if audit.ambient_authority_visible != 0 {
        return false
    }
    return audit.compiler_reopening_visible == 0
}

func validate_phase125_invalidation_and_rejection_audit(audit: Phase125InvalidationAudit, scheduler_contract_hardened: u32, lifecycle_contract_hardened: u32, capability_contract_hardened: u32, ipc_contract_hardened: u32, address_space_contract_hardened: u32, interrupt_contract_hardened: u32, timer_contract_hardened: u32, barrier_contract_hardened: u32) bool {
    if !validate_phase124_delegation_chain_stress(audit.phase124, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened, barrier_contract_hardened) {
        return false
    }
    if audit.invalidated_holder_pid != audit.phase124.final_holder_pid {
        return false
    }
    if audit.control_endpoint_id != audit.phase124.control_endpoint_id {
        return false
    }
    if audit.invalidated_endpoint_id != audit.phase124.delegated_endpoint_id {
        return false
    }
    if audit.invalidated_handle_slot != audit.phase124.final_receive_handle_slot {
        return false
    }
    if audit.invalidated_handle_slot == 0 {
        return false
    }
    if syscall.status_score(audit.rejected_send_status) != 8 {
        return false
    }
    if syscall.status_score(audit.rejected_receive_status) != 8 {
        return false
    }
    if syscall.status_score(audit.surviving_control_send_status) != 2 {
        return false
    }
    if audit.surviving_control_source_pid != audit.invalidated_holder_pid {
        return false
    }
    if audit.surviving_control_queue_depth != 1 {
        return false
    }
    if audit.authority_loss_visible != 1 {
        return false
    }
    if audit.broader_revocation_visible != 0 {
        return false
    }
    return audit.compiler_reopening_visible == 0
}

func validate_phase126_authority_lifetime_classification(audit: Phase126AuthorityLifetimeAudit, scheduler_contract_hardened: u32, lifecycle_contract_hardened: u32, capability_contract_hardened: u32, ipc_contract_hardened: u32, address_space_contract_hardened: u32, interrupt_contract_hardened: u32, timer_contract_hardened: u32, barrier_contract_hardened: u32) bool {
    if !validate_phase125_invalidation_and_rejection_audit(audit.phase125, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened, barrier_contract_hardened) {
        return false
    }
    if audit.classified_holder_pid != audit.phase125.invalidated_holder_pid {
        return false
    }
    if audit.long_lived_endpoint_id != audit.phase125.control_endpoint_id {
        return false
    }
    if audit.short_lived_endpoint_id != audit.phase125.invalidated_endpoint_id {
        return false
    }
    if audit.long_lived_handle_slot == 0 {
        return false
    }
    if audit.short_lived_handle_slot == 0 {
        return false
    }
    if audit.long_lived_handle_slot == audit.short_lived_handle_slot {
        return false
    }
    if audit.short_lived_handle_slot != audit.phase125.invalidated_handle_slot {
        return false
    }
    if syscall.status_score(audit.phase125.surviving_control_send_status) != 2 {
        return false
    }
    if audit.phase125.surviving_control_source_pid != audit.classified_holder_pid {
        return false
    }
    if audit.phase125.surviving_control_queue_depth != 1 {
        return false
    }
    if syscall.status_score(audit.repeat_long_lived_send_status) != 2 {
        return false
    }
    if audit.repeat_long_lived_source_pid != audit.classified_holder_pid {
        return false
    }
    if audit.repeat_long_lived_queue_depth != 2 {
        return false
    }
    if audit.long_lived_class_visible != 1 {
        return false
    }
    if audit.short_lived_class_visible != 1 {
        return false
    }
    if audit.broader_lifetime_framework_visible != 0 {
        return false
    }
    return audit.compiler_reopening_visible == 0
}