import shell_service
import syscall

func validate_phase141_interactive_service_system_scope_freeze(audit: Phase141InteractiveServiceSystemScopeFreezeAudit, scheduler_contract_hardened: u32, lifecycle_contract_hardened: u32, capability_contract_hardened: u32, ipc_contract_hardened: u32, address_space_contract_hardened: u32, interrupt_contract_hardened: u32, timer_contract_hardened: u32, barrier_contract_hardened: u32) bool {
    if !validate_phase140_serial_ingress_composed_service_graph(audit.phase140, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened, barrier_contract_hardened) {
        return false
    }
    if audit.serial_service_pid == 0 || audit.shell_service_pid == 0 || audit.kv_service_pid == 0 {
        return false
    }
    if audit.serial_service_pid == audit.shell_service_pid || audit.shell_service_pid == audit.kv_service_pid {
        return false
    }
    if audit.serial_forward_endpoint_id == 0 || audit.serial_forward_endpoint_id != audit.shell_endpoint_id {
        return false
    }
    if syscall.status_score(audit.serial_forward_status) != 2 {
        return false
    }
    if audit.serial_forward_request_len != audit.shell_request_len {
        return false
    }
    if audit.serial_forward_request_byte0 != audit.shell_request_byte0 || audit.serial_forward_request_byte1 != audit.shell_request_byte1 {
        return false
    }
    if shell_service.tag_score(audit.shell_tag) != 8 {
        return false
    }
    if audit.echo_endpoint_id == 0 || audit.log_endpoint_id == 0 || audit.kv_endpoint_id == 0 {
        return false
    }
    if audit.echo_endpoint_id == audit.log_endpoint_id || audit.echo_endpoint_id == audit.kv_endpoint_id || audit.log_endpoint_id == audit.kv_endpoint_id {
        return false
    }
    if audit.echo_route_count != 0 || audit.log_route_count != 1 || audit.kv_route_count != 0 {
        return false
    }
    if audit.invalid_command_count != 0 {
        return false
    }
    if syscall.status_score(audit.shell_reply_status) != 2 {
        return false
    }
    if audit.shell_reply_len != 2 {
        return false
    }
    if audit.fixed_vocabulary_visible == 0 {
        return false
    }
    if audit.kernel_broker_visible != 0 {
        return false
    }
    if audit.dynamic_routing_visible != 0 {
        return false
    }
    if audit.general_shell_framework_visible != 0 {
        return false
    }
    return audit.compiler_reopening_visible == 0
}