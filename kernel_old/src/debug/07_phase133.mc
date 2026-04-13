import ipc
import state
import syscall

func validate_phase133_message_lifetime_and_reuse(audit: Phase133MessageLifetimeAudit, scheduler_contract_hardened: u32, lifecycle_contract_hardened: u32, capability_contract_hardened: u32, ipc_contract_hardened: u32, address_space_contract_hardened: u32, interrupt_contract_hardened: u32, timer_contract_hardened: u32, barrier_contract_hardened: u32) bool {
    if !validate_phase132_backpressure_and_blocking(audit.phase132, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened, barrier_contract_hardened) {
        return false
    }
    if audit.endpoint_id == 0 || audit.attached_endpoint_id == 0 {
        return false
    }
    if syscall.status_score(audit.first_send_status) != 2 {
        return false
    }
    if audit.source_handle_live_after_first_send != 0 {
        return false
    }
    if syscall.status_score(audit.first_receive_status) != 2 {
        return false
    }
    if audit.first_receive_byte0 != 84 {
        return false
    }
    if audit.first_receive_handle_slot != 3 {
        return false
    }
    if audit.first_receive_handle_count != 1 {
        return false
    }
    if audit.first_received_handle_endpoint_id != audit.attached_endpoint_id {
        return false
    }
    if ipc.message_state_score(audit.first_retired_slot_state) != 1 {
        return false
    }
    if audit.first_retired_slot_payload0 != 0 {
        return false
    }
    if syscall.status_score(audit.second_send_status) != 2 {
        return false
    }
    if syscall.status_score(audit.second_receive_status) != 2 {
        return false
    }
    if audit.second_receive_byte0 != 82 {
        return false
    }
    if ipc.message_state_score(audit.second_retired_slot_state) != 1 {
        return false
    }
    if audit.second_retired_slot_payload0 != 0 {
        return false
    }
    if syscall.status_score(audit.abort_transfer_send_status) != 2 {
        return false
    }
    if audit.abort_transfer_source_handle_live_after_send != 0 {
        return false
    }
    if audit.abort_transfer_aborted_messages != 1 {
        return false
    }
    if ipc.message_state_score(audit.abort_transfer_scrubbed_state) != 1 {
        return false
    }
    if audit.abort_transfer_scrubbed_payload0 != 0 {
        return false
    }
    if audit.abort_transfer_scrubbed_attached_count != 0 {
        return false
    }
    if audit.abort_transfer_scrubbed_attached_endpoint_id != 0 {
        return false
    }
    if audit.abort_transfer_scrubbed_source_handle_slot != 0 {
        return false
    }
    if syscall.status_score(audit.close_blocked_send_status) != 4 {
        return false
    }
    if state.task_state_score(audit.close_blocked_send_state) != 32 {
        return false
    }
    if audit.close_aborted_messages != 2 {
        return false
    }
    if audit.close_wake_count != 1 {
        return false
    }
    if ipc.wake_reason_score(audit.close_wake_reason) != 8 {
        return false
    }
    if audit.close_wake_task_id != audit.phase132.sender_tid {
        return false
    }
    if audit.close_ready_count != 1 {
        return false
    }
    if state.task_state_score(audit.close_task_state) != 4 {
        return false
    }
    if syscall.status_score(audit.closed_send_status) != 32 {
        return false
    }
    if syscall.status_score(audit.closed_receive_status) != 32 {
        return false
    }
    if audit.owner_death_closed_count != 2 {
        return false
    }
    if audit.owner_death_aborted_messages != 1 {
        return false
    }
    if audit.owner_death_wake_count != 1 {
        return false
    }
    if ipc.wake_reason_score(audit.owner_death_wake_reason) != 8 {
        return false
    }
    if audit.owner_death_transfer_source_handle_live_after_send != 0 {
        return false
    }
    if ipc.message_state_score(audit.owner_death_transfer_scrubbed_state) != 1 {
        return false
    }
    if audit.owner_death_transfer_scrubbed_payload0 != 0 {
        return false
    }
    if audit.owner_death_transfer_scrubbed_attached_count != 0 {
        return false
    }
    if audit.owner_death_transfer_scrubbed_attached_endpoint_id != 0 {
        return false
    }
    if audit.owner_death_transfer_scrubbed_source_handle_slot != 0 {
        return false
    }
    if audit.kernel_policy_visible != 0 {
        return false
    }
    return audit.compiler_reopening_visible == 0
}