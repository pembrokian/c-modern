import syscall

func validate_phase131_fan_out_composition(audit: Phase131FanOutCompositionAudit, scheduler_contract_hardened: u32, lifecycle_contract_hardened: u32, capability_contract_hardened: u32, ipc_contract_hardened: u32, address_space_contract_hardened: u32, interrupt_contract_hardened: u32, timer_contract_hardened: u32, barrier_contract_hardened: u32) bool {
    if !validate_phase130_explicit_restart_or_replacement(audit.phase130, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened, barrier_contract_hardened) {
        return false
    }
    if audit.composition_policy_owner_pid != audit.phase130.replacement_policy_owner_pid {
        return false
    }
    if audit.composition_policy_owner_pid == 0 {
        return false
    }
    if audit.composition_service_pid == 0 {
        return false
    }
    if audit.composition_service_key == 0 {
        return false
    }
    if audit.composition_service_key == audit.phase130.replacement_service_key {
        return false
    }
    if audit.composition_wait_handle_slot == 0 {
        return false
    }
    if audit.fixed_directory_entry_count != audit.phase130.directory_entry_count + 1 {
        return false
    }
    if audit.fixed_directory_entry_count != 4 {
        return false
    }
    if audit.control_endpoint_id == 0 {
        return false
    }
    if audit.echo_endpoint_id == 0 {
        return false
    }
    if audit.log_endpoint_id == 0 {
        return false
    }
    if audit.control_endpoint_id == audit.echo_endpoint_id || audit.control_endpoint_id == audit.log_endpoint_id {
        return false
    }
    if audit.echo_endpoint_id == audit.log_endpoint_id {
        return false
    }
    if syscall.status_score(audit.request_receive_status) != 2 {
        return false
    }
    if syscall.status_score(audit.echo_fanout_status) != 2 {
        return false
    }
    if syscall.status_score(audit.log_fanout_status) != 2 {
        return false
    }
    if syscall.status_score(audit.echo_reply_status) != 2 {
        return false
    }
    if syscall.status_score(audit.log_ack_status) != 2 {
        return false
    }
    if syscall.status_score(audit.aggregate_reply_status) != 2 {
        return false
    }
    if syscall.status_score(audit.composition_wait_status) != 2 {
        return false
    }
    if audit.echo_fanout_endpoint_id != audit.echo_endpoint_id {
        return false
    }
    if audit.log_fanout_endpoint_id != audit.log_endpoint_id {
        return false
    }
    if audit.aggregate_reply_byte0 != audit.phase130.phase129.phase128.phase126.phase125.phase124.phase123.phase122.phase121.phase120.phase119.phase118.phase117.running_slice.echo_service_request_byte0 {
        return false
    }
    if audit.aggregate_reply_byte1 != audit.phase130.phase129.phase128.phase126.phase125.phase124.phase123.phase122.phase121.phase120.phase119.phase118.phase117.running_slice.echo_service_request_byte1 {
        return false
    }
    if audit.aggregate_reply_byte2 != audit.phase130.phase129.phase128.phase126.phase125.phase124.phase123.phase122.phase121.phase120.phase119.phase118.phase117.log_service_handshake.ack_byte {
        return false
    }
    if audit.aggregate_reply_byte3 != 2 {
        return false
    }
    if audit.explicit_composition_visible != 1 {
        return false
    }
    if audit.kernel_broker_visible != 0 {
        return false
    }
    if audit.dynamic_namespace_visible != 0 {
        return false
    }
    return audit.compiler_reopening_visible == 0
}