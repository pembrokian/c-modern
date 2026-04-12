import shell_service
import syscall

func validate_phase142_serial_shell_command_routing(audit: Phase142SerialShellCommandRoutingAudit, scheduler_contract_hardened: u32, lifecycle_contract_hardened: u32, capability_contract_hardened: u32, ipc_contract_hardened: u32, address_space_contract_hardened: u32, interrupt_contract_hardened: u32, timer_contract_hardened: u32, barrier_contract_hardened: u32) bool {
    if !validate_phase141_interactive_service_system_scope_freeze(audit.phase141, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened, barrier_contract_hardened) {
        return false
    }
    if audit.serial_service_pid == 0 || audit.shell_service_pid == 0 || audit.kv_service_pid == 0 {
        return false
    }
    if audit.serial_service_pid == audit.shell_service_pid || audit.shell_service_pid == audit.kv_service_pid {
        return false
    }
    if audit.serial_forward_endpoint_id != audit.phase141.shell_endpoint_id {
        return false
    }
    if syscall.status_score(audit.serial_forward_status) != 2 {
        return false
    }
    if audit.serial_forward_count != 6 {
        return false
    }
    if syscall.status_score(audit.serial_final_reply_status) != 2 {
        return false
    }
    if audit.serial_final_reply_len != 1 || audit.serial_final_reply_byte0 != 63 || audit.serial_final_reply_count != 6 {
        return false
    }
    if shell_service.tag_score(audit.echo_route.tag) != 2 {
        return false
    }
    if shell_service.tag_score(audit.log_append_route.tag) != 4 {
        return false
    }
    if shell_service.tag_score(audit.log_tail_route.tag) != 8 {
        return false
    }
    if shell_service.tag_score(audit.kv_set_route.tag) != 16 {
        return false
    }
    if shell_service.tag_score(audit.kv_get_route.tag) != 32 {
        return false
    }
    if shell_service.tag_score(audit.invalid_route.tag) != 64 {
        return false
    }
    if audit.echo_route.echo_route_count != 1 || audit.log_append_route.log_route_count != 1 || audit.log_tail_route.log_route_count != 2 {
        return false
    }
    if audit.kv_set_route.kv_route_count != 1 || audit.kv_get_route.kv_route_count != 2 || audit.invalid_route.invalid_command_count != 1 {
        return false
    }
    if audit.echo_route.reply_byte0 != 72 || audit.echo_route.reply_byte1 != 73 {
        return false
    }
    if audit.log_append_route.reply_byte0 != 33 {
        return false
    }
    if audit.log_tail_route.reply_byte0 != 80 || audit.log_tail_route.reply_byte1 != 33 {
        return false
    }
    if audit.kv_set_route.reply_byte0 != 75 || audit.kv_set_route.reply_byte1 != 86 {
        return false
    }
    if audit.kv_get_route.reply_byte0 != 75 || audit.kv_get_route.reply_byte1 != 86 {
        return false
    }
    if audit.invalid_route.reply_byte0 != 63 || audit.invalid_route.reply_len != 1 {
        return false
    }
    if audit.compact_command_encoding_visible == 0 || audit.malformed_command_visible == 0 {
        return false
    }
    if audit.general_shell_framework_visible != 0 {
        return false
    }
    if audit.payload_width_reopened_visible != 0 {
        return false
    }
    return audit.compiler_reopening_visible == 0
}