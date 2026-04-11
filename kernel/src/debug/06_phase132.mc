import ipc
import state
import syscall

func validate_phase132_backpressure_and_blocking(audit: Phase132BackpressureAudit, scheduler_contract_hardened: u32, lifecycle_contract_hardened: u32, capability_contract_hardened: u32, ipc_contract_hardened: u32, address_space_contract_hardened: u32, interrupt_contract_hardened: u32, timer_contract_hardened: u32, barrier_contract_hardened: u32) bool {
    if !validate_phase131_fan_out_composition(audit.phase131, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened, barrier_contract_hardened) {
        return false
    }
    if audit.endpoint_id == 0 {
        return false
    }
    if audit.sender_pid == 0 || audit.receiver_pid == 0 {
        return false
    }
    if audit.sender_tid == 0 || audit.receiver_tid == 0 {
        return false
    }
    if audit.sender_task_slot == 0 || audit.receiver_task_slot == 0 {
        return false
    }
    if audit.sender_task_slot == audit.receiver_task_slot {
        return false
    }
    if syscall.status_score(audit.blocked_send_status) != 4 {
        return false
    }
    if syscall.block_reason_score(audit.blocked_send_reason) != 2 {
        return false
    }
    if state.task_state_score(audit.blocked_send_task_state) != 32 {
        return false
    }
    if audit.blocked_send_queue_depth != 2 {
        return false
    }
    if audit.blocked_send_waiter_task_slot != audit.sender_task_slot {
        return false
    }
    if syscall.status_score(audit.sender_wake_status) != 2 {
        return false
    }
    if state.task_state_score(audit.sender_wake_task_state) != 4 {
        return false
    }
    if ipc.wake_reason_score(audit.sender_wake_reason) != 2 {
        return false
    }
    if audit.sender_wake_task_id != audit.sender_tid {
        return false
    }
    if audit.sender_wake_ready_count != 1 {
        return false
    }
    if audit.sender_wake_queue_depth != 1 {
        return false
    }
    if audit.sender_wake_waiter_task_slot != 0 {
        return false
    }
    if syscall.status_score(audit.blocked_receive_status) != 4 {
        return false
    }
    if syscall.block_reason_score(audit.blocked_receive_reason) != 4 {
        return false
    }
    if state.task_state_score(audit.blocked_receive_task_state) != 64 {
        return false
    }
    if audit.blocked_receive_queue_depth != 0 {
        return false
    }
    if audit.blocked_receive_waiter_task_slot != audit.receiver_task_slot {
        return false
    }
    if syscall.status_score(audit.receiver_wake_status) != 2 {
        return false
    }
    if state.task_state_score(audit.receiver_wake_task_state) != 4 {
        return false
    }
    if ipc.wake_reason_score(audit.receiver_wake_reason) != 4 {
        return false
    }
    if audit.receiver_wake_task_id != audit.receiver_tid {
        return false
    }
    if audit.receiver_wake_ready_count != 1 {
        return false
    }
    if audit.receiver_wake_queue_depth != 1 {
        return false
    }
    if audit.receiver_wake_waiter_task_slot != 0 {
        return false
    }
    if audit.kernel_policy_visible != 0 {
        return false
    }
    return audit.compiler_reopening_visible == 0
}