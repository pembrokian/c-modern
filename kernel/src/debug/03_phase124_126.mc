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