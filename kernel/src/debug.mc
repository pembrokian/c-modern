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