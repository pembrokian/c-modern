import init

func validate_phase148_authority_ergonomics_under_reuse(audit: Phase148AuthorityErgonomicsAudit, scheduler_contract_hardened: u32, lifecycle_contract_hardened: u32, capability_contract_hardened: u32, ipc_contract_hardened: u32, address_space_contract_hardened: u32, interrupt_contract_hardened: u32, timer_contract_hardened: u32, barrier_contract_hardened: u32) bool {
    if !validate_phase147_ipc_shape_audit_under_real_usage(audit.phase147, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened, barrier_contract_hardened) {
        return false
    }
    if audit.serial_endpoint_handle_slot == 0 || audit.shell_endpoint_handle_slot == 0 || audit.log_endpoint_handle_slot == 0 || audit.kv_endpoint_handle_slot == 0 {
        return false
    }
    if audit.serial_endpoint_handle_slot != 1 || audit.shell_endpoint_handle_slot != 1 || audit.log_endpoint_handle_slot != 1 || audit.kv_endpoint_handle_slot != 1 {
        return false
    }
    if audit.phase147.repeated_log_tail_route.log_route_count != 3 || audit.phase147.repeated_kv_get_route.log_route_count != 3 {
        return false
    }
    if audit.phase147.repeated_log_tail_route.kv_route_count != 2 || audit.phase147.repeated_kv_get_route.kv_route_count != 3 {
        return false
    }
    if audit.phase147.phase146.restart.owner_pid == 0 || audit.phase147.phase146.restart.service_key == 0 {
        return false
    }
    if audit.phase147.phase146.restart.previous_service_pid != audit.phase147.kv_service_pid {
        return false
    }
    if audit.phase147.phase146.restart.replacement_service_pid != audit.phase147.kv_service_pid {
        return false
    }
    if audit.phase147.phase146.restart.restart_count != 1 || audit.phase147.phase146.restart.retained_boot_wiring_visible == 0 {
        return false
    }
    if audit.phase147.phase146.restart.shared_control_endpoint_id != 1 {
        return false
    }
    if audit.explicit_slot_authority_visible == 0 || audit.retained_state_authority_local_visible == 0 || audit.restart_handoff_explicit_visible == 0 || audit.repeated_authority_ceremony_stable_visible == 0 || audit.narrow_capability_helper_visible == 0 {
        return false
    }
    if audit.overscoped_helper_visible != 0 || audit.ambient_authority_visible != 0 {
        return false
    }
    return audit.compiler_reopening_visible == 0
}