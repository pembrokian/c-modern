import log_service
import shell_service

func validate_phase143_long_lived_log_service(audit: Phase143LongLivedLogServiceAudit, scheduler_contract_hardened: u32, lifecycle_contract_hardened: u32, capability_contract_hardened: u32, ipc_contract_hardened: u32, address_space_contract_hardened: u32, interrupt_contract_hardened: u32, timer_contract_hardened: u32, barrier_contract_hardened: u32) bool {
    if !validate_phase142_serial_shell_command_routing(audit.phase142, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened, barrier_contract_hardened) {
        return false
    }
    if audit.log_service_pid == 0 || audit.shell_service_pid == 0 {
        return false
    }
    if audit.log_service_pid == audit.shell_service_pid {
        return false
    }
    if shell_service.tag_score(audit.overflow_append_route.tag) != 4 {
        return false
    }
    if shell_service.tag_score(audit.tail_route.tag) != 8 {
        return false
    }
    if audit.overflow_append_route.reply_byte0 != 33 || audit.overflow_append_route.reply_byte1 != 1 {
        return false
    }
    if audit.overflow_append_route.log_route_count != 5 {
        return false
    }
    if audit.tail_route.log_route_count != 6 {
        return false
    }
    if audit.tail_route.reply_byte0 != 69 || audit.tail_route.reply_byte1 != 68 {
        return false
    }
    if log_service.tag_score(audit.retention.tag) != 8 {
        return false
    }
    if audit.retention.retained_len != 4 {
        return false
    }
    if audit.retention.append_count != 5 || audit.retention.tail_query_count != 1 {
        return false
    }
    if audit.retention.overwrite_count != 1 || audit.retention.overflowed_last_request != 0 {
        return false
    }
    if audit.retention.retained_byte0 != 66 || audit.retention.retained_byte1 != 67 {
        return false
    }
    if audit.retention.retained_byte2 != 68 || audit.retention.retained_byte3 != 69 {
        return false
    }
    if audit.retention.tail_byte0 != 69 || audit.retention.tail_byte1 != 68 {
        return false
    }
    if audit.bounded_retention_visible == 0 || audit.explicit_overwrite_policy_visible == 0 {
        return false
    }
    if audit.durable_persistence_visible != 0 {
        return false
    }
    return audit.compiler_reopening_visible == 0
}