import kv_service
import log_service
import shell_service

func validate_phase144_stateful_key_value_service_follow_through(audit: Phase144StatefulKeyValueServiceFollowThroughAudit, scheduler_contract_hardened: u32, lifecycle_contract_hardened: u32, capability_contract_hardened: u32, ipc_contract_hardened: u32, address_space_contract_hardened: u32, interrupt_contract_hardened: u32, timer_contract_hardened: u32, barrier_contract_hardened: u32) bool {
    if !validate_phase143_long_lived_log_service(audit.phase143, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened, barrier_contract_hardened) {
        return false
    }
    if audit.kv_service_pid == 0 || audit.shell_service_pid == 0 || audit.log_service_pid == 0 {
        return false
    }
    if audit.kv_service_pid == audit.shell_service_pid || audit.kv_service_pid == audit.log_service_pid || audit.shell_service_pid == audit.log_service_pid {
        return false
    }
    if shell_service.tag_score(audit.missing_get_route.tag) != 32 {
        return false
    }
    if shell_service.tag_score(audit.initial_set_route.tag) != 16 {
        return false
    }
    if shell_service.tag_score(audit.hit_get_route.tag) != 32 {
        return false
    }
    if shell_service.tag_score(audit.overwrite_set_route.tag) != 16 {
        return false
    }
    if shell_service.tag_score(audit.overwrite_get_route.tag) != 32 {
        return false
    }
    if audit.missing_get_route.reply_byte0 != 75 || audit.missing_get_route.reply_byte1 != 0 || audit.missing_get_route.reply_byte2 != 0 || audit.missing_get_route.reply_byte3 != 1 {
        return false
    }
    if audit.initial_set_route.reply_byte0 != 75 || audit.initial_set_route.reply_byte1 != 86 || audit.initial_set_route.reply_byte2 != 0 || audit.initial_set_route.reply_byte3 != 0 {
        return false
    }
    if audit.hit_get_route.reply_byte0 != 75 || audit.hit_get_route.reply_byte1 != 86 || audit.hit_get_route.reply_byte2 != 1 || audit.hit_get_route.reply_byte3 != 0 {
        return false
    }
    if audit.overwrite_set_route.reply_byte0 != 75 || audit.overwrite_set_route.reply_byte1 != 87 || audit.overwrite_set_route.reply_byte2 != 1 || audit.overwrite_set_route.reply_byte3 != 0 {
        return false
    }
    if audit.overwrite_get_route.reply_byte0 != 75 || audit.overwrite_get_route.reply_byte1 != 87 || audit.overwrite_get_route.reply_byte2 != 1 || audit.overwrite_get_route.reply_byte3 != 0 {
        return false
    }
    if kv_service.tag_score(audit.kv_retention.tag) != 4 {
        return false
    }
    if audit.kv_retention.request_count != 5 || audit.kv_retention.set_count != 2 || audit.kv_retention.get_count != 3 {
        return false
    }
    if audit.kv_retention.overwrite_count != 1 || audit.kv_retention.missing_count != 1 {
        return false
    }
    if audit.kv_retention.retained_count != 1 || audit.kv_retention.found == 0 {
        return false
    }
    if audit.kv_retention.key_byte != 75 || audit.kv_retention.value_byte != 87 {
        return false
    }
    if audit.kv_retention.retained_key0 != 75 || audit.kv_retention.retained_value0 != 87 {
        return false
    }
    if audit.kv_retention.logged_write_count != 2 {
        return false
    }
    if log_service.tag_score(audit.write_log_retention.tag) != 16 {
        return false
    }
    if audit.write_log_retention.retained_len != 2 || audit.write_log_retention.append_count != 2 {
        return false
    }
    if audit.write_log_retention.retained_byte0 != 83 || audit.write_log_retention.retained_byte1 != 79 {
        return false
    }
    if audit.write_log_retention.tail_byte0 != 79 || audit.write_log_retention.tail_byte1 != 83 {
        return false
    }
    if audit.bounded_retention_visible == 0 || audit.missing_key_visible == 0 || audit.explicit_overwrite_visible == 0 || audit.fixed_write_log_visible == 0 {
        return false
    }
    if audit.durable_persistence_visible != 0 {
        return false
    }
    return audit.compiler_reopening_visible == 0
}