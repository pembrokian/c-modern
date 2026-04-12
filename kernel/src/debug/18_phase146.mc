import syscall

func validate_phase146_service_shape_consolidation(audit: Phase146ServiceShapeConsolidationAudit, scheduler_contract_hardened: u32, lifecycle_contract_hardened: u32, capability_contract_hardened: u32, ipc_contract_hardened: u32, address_space_contract_hardened: u32, interrupt_contract_hardened: u32, timer_contract_hardened: u32, barrier_contract_hardened: u32) bool {
    if !validate_phase145_service_restart_failure_and_usage_pressure(audit.phase145, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened, barrier_contract_hardened) {
        return false
    }
    if audit.serial_service_pid == 0 || audit.shell_service_pid == 0 || audit.log_service_pid == 0 || audit.kv_service_pid == 0 || audit.init_policy_owner_pid == 0 {
        return false
    }
    if audit.serial_service_pid == audit.shell_service_pid || audit.serial_service_pid == audit.log_service_pid || audit.serial_service_pid == audit.kv_service_pid {
        return false
    }
    if audit.shell_service_pid == audit.log_service_pid || audit.shell_service_pid == audit.kv_service_pid || audit.log_service_pid == audit.kv_service_pid {
        return false
    }
    if audit.serial_endpoint_handle_slot != 1 || audit.shell_endpoint_handle_slot != 1 || audit.log_endpoint_handle_slot != 1 || audit.kv_endpoint_handle_slot != 1 {
        return false
    }
    if audit.serial_forward.service_pid != audit.serial_service_pid {
        return false
    }
    if audit.shell_route.service_pid != audit.shell_service_pid || audit.log_retention.service_pid != audit.log_service_pid || audit.kv_retention.service_pid != audit.kv_service_pid {
        return false
    }
    if audit.restart.owner_pid != audit.init_policy_owner_pid {
        return false
    }
    if audit.serial_forward.forward_endpoint_id == 0 || audit.serial_forward.forward_endpoint_id != audit.shell_route.endpoint_id {
        return false
    }
    if syscall.status_score(audit.serial_forward.forward_status) != 2 {
        return false
    }
    if audit.serial_forward.forwarded_request_len != audit.shell_route.request_len {
        return false
    }
    if audit.serial_forward.forwarded_request_byte0 != audit.shell_route.request_byte0 || audit.serial_forward.forwarded_request_byte1 != audit.shell_route.request_byte1 {
        return false
    }
    if audit.shell_route.client_pid != audit.serial_service_pid {
        return false
    }
    if audit.shell_route.echo_endpoint_id == 0 || audit.shell_route.log_endpoint_id == 0 || audit.shell_route.kv_endpoint_id == 0 {
        return false
    }
    if audit.shell_route.echo_endpoint_id == audit.shell_route.log_endpoint_id || audit.shell_route.echo_endpoint_id == audit.shell_route.kv_endpoint_id || audit.shell_route.log_endpoint_id == audit.shell_route.kv_endpoint_id {
        return false
    }
    if audit.shell_route.kv_route_count != 4 || audit.shell_route.echo_route_count != 0 || audit.shell_route.log_route_count != 0 || audit.shell_route.invalid_command_count != 0 {
        return false
    }
    if syscall.status_score(audit.shell_route.reply_status) != 2 {
        return false
    }
    if audit.shell_route.reply_len != audit.phase145.restarted_get_route.reply_len {
        return false
    }
    if audit.shell_route.reply_byte0 != audit.phase145.restarted_get_route.reply_byte0 || audit.shell_route.reply_byte1 != audit.phase145.restarted_get_route.reply_byte1 || audit.shell_route.reply_byte2 != audit.phase145.restarted_get_route.reply_byte2 || audit.shell_route.reply_byte3 != audit.phase145.restarted_get_route.reply_byte3 {
        return false
    }
    if audit.log_retention.retained_len != audit.phase145.event_log_retention.retained_len || audit.log_retention.append_count != audit.phase145.event_log_retention.append_count || audit.log_retention.tail_query_count != audit.phase145.event_log_retention.tail_query_count || audit.log_retention.overwrite_count != audit.phase145.event_log_retention.overwrite_count {
        return false
    }
    if audit.log_retention.retained_byte0 != audit.phase145.event_log_retention.retained_byte0 || audit.log_retention.retained_byte1 != audit.phase145.event_log_retention.retained_byte1 || audit.log_retention.retained_byte2 != audit.phase145.event_log_retention.retained_byte2 || audit.log_retention.tail_byte0 != audit.phase145.event_log_retention.tail_byte0 || audit.log_retention.tail_byte1 != audit.phase145.event_log_retention.tail_byte1 {
        return false
    }
    if audit.kv_retention.available != audit.phase145.post_restart_retention.available || audit.kv_retention.retained_count != audit.phase145.post_restart_retention.retained_count || audit.kv_retention.get_count != audit.phase145.post_restart_retention.get_count || audit.kv_retention.missing_count != audit.phase145.post_restart_retention.missing_count || audit.kv_retention.unavailable_count != audit.phase145.post_restart_retention.unavailable_count {
        return false
    }
    if audit.kv_retention.retained_key0 != audit.phase145.post_restart_retention.retained_key0 || audit.kv_retention.retained_value0 != audit.phase145.post_restart_retention.retained_value0 {
        return false
    }
    if audit.restart.service_key != audit.phase145.restart.service_key || audit.restart.previous_service_pid != audit.phase145.restart.previous_service_pid || audit.restart.replacement_service_pid != audit.phase145.restart.replacement_service_pid || audit.restart.shared_control_endpoint_id != audit.phase145.restart.shared_control_endpoint_id || audit.restart.restart_count != audit.phase145.restart.restart_count || audit.restart.retained_boot_wiring_visible != audit.phase145.restart.retained_boot_wiring_visible {
        return false
    }
    if audit.fixed_boot_wiring_visible == 0 || audit.service_local_request_reply_visible == 0 || audit.shell_syntax_boundary_visible == 0 || audit.service_semantic_boundary_visible == 0 || audit.explicit_failure_exposure_visible == 0 || audit.init_owned_restart_visible == 0 || audit.intentional_shell_difference_visible == 0 || audit.intentional_stateful_difference_visible == 0 {
        return false
    }
    if audit.dynamic_service_framework_visible != 0 || audit.dynamic_registration_visible != 0 {
        return false
    }
    return audit.compiler_reopening_visible == 0
}