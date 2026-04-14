import kv_service
import log_service
import shell_service

func validate_phase145_service_restart_failure_and_usage_pressure(audit: Phase145ServiceRestartFailureAndUsagePressureAudit, scheduler_contract_hardened: u32, lifecycle_contract_hardened: u32, capability_contract_hardened: u32, ipc_contract_hardened: u32, address_space_contract_hardened: u32, interrupt_contract_hardened: u32, timer_contract_hardened: u32, barrier_contract_hardened: u32) bool {
    if !validate_phase144_stateful_key_value_service_follow_through(audit.phase144, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened, barrier_contract_hardened) {
        return false
    }
    if audit.kv_service_pid == 0 || audit.shell_service_pid == 0 || audit.log_service_pid == 0 || audit.init_policy_owner_pid == 0 {
        return false
    }
    if audit.kv_service_pid == audit.shell_service_pid || audit.kv_service_pid == audit.log_service_pid || audit.shell_service_pid == audit.log_service_pid {
        return false
    }
    if shell_service.tag_score(audit.pre_failure_set_route.tag) != 16 {
        return false
    }
    if shell_service.tag_score(audit.pre_failure_get_route.tag) != 32 {
        return false
    }
    if shell_service.tag_score(audit.failed_get_route.tag) != 32 {
        return false
    }
    if shell_service.tag_score(audit.restarted_get_route.tag) != 32 {
        return false
    }
    if audit.pre_failure_set_route.reply_byte0 != 75 || audit.pre_failure_set_route.reply_byte1 != 86 || audit.pre_failure_set_route.reply_byte2 != 0 || audit.pre_failure_set_route.reply_byte3 != 0 {
        return false
    }
    if audit.pre_failure_get_route.reply_byte0 != 75 || audit.pre_failure_get_route.reply_byte1 != 86 || audit.pre_failure_get_route.reply_byte2 != 1 || audit.pre_failure_get_route.reply_byte3 != 0 {
        return false
    }
    if audit.failed_get_route.reply_byte0 != 75 || audit.failed_get_route.reply_byte1 != 0 || audit.failed_get_route.reply_byte2 != 0 || audit.failed_get_route.reply_byte3 != 2 {
        return false
    }
    if audit.restarted_get_route.reply_byte0 != 75 || audit.restarted_get_route.reply_byte1 != 0 || audit.restarted_get_route.reply_byte2 != 0 || audit.restarted_get_route.reply_byte3 != 1 {
        return false
    }
    if syscall.status_score(audit.failed_get_route.reply_status) != 2 || syscall.status_score(audit.restarted_get_route.reply_status) != 2 {
        return false
    }
    if kv_service.tag_score(audit.pre_failure_retention.tag) != 4 {
        return false
    }
    if audit.pre_failure_retention.available == 0 || audit.pre_failure_retention.retained_count != 1 {
        return false
    }
    if audit.pre_failure_retention.retained_key0 != 75 || audit.pre_failure_retention.retained_value0 != 86 {
        return false
    }
    if kv_service.tag_score(audit.post_restart_retention.tag) != 4 {
        return false
    }
    if audit.post_restart_retention.available == 0 || audit.post_restart_retention.retained_count != 0 {
        return false
    }
    if audit.post_restart_retention.get_count != 1 || audit.post_restart_retention.missing_count != 1 {
        return false
    }
    if audit.post_restart_retention.unavailable_count != 0 || audit.post_restart_retention.unavailable_last_request != 0 {
        return false
    }
    if audit.restart.owner_pid != audit.init_policy_owner_pid {
        return false
    }
    if audit.restart.service_key != 6 {
        return false
    }
    if audit.restart.previous_service_pid != audit.kv_service_pid || audit.restart.replacement_service_pid != audit.kv_service_pid {
        return false
    }
    if audit.restart.shared_control_endpoint_id != audit.phase144.phase143.phase142.phase141.phase140.phase137.phase136.phase131.phase130.phase129.shared_control_endpoint_id {
        return false
    }
    if audit.restart.restart_count != 1 || audit.restart.retained_boot_wiring_visible == 0 {
        return false
    }
    if log_service.tag_score(audit.event_log_retention.tag) != 16 {
        return false
    }
    if audit.event_log_retention.retained_len != 3 || audit.event_log_retention.append_count != 3 {
        return false
    }
    if audit.event_log_retention.retained_byte0 != 83 || audit.event_log_retention.retained_byte1 != 70 || audit.event_log_retention.retained_byte2 != 82 {
        return false
    }
    if audit.event_log_retention.tail_byte0 != 82 || audit.event_log_retention.tail_byte1 != 70 {
        return false
    }
    if audit.shell_failure_visible == 0 || audit.init_restart_visible == 0 || audit.post_restart_state_reset_visible == 0 || audit.manual_retry_visible == 0 {
        return false
    }
    if audit.kernel_supervision_visible != 0 || audit.durable_persistence_visible != 0 {
        return false
    }
    return audit.compiler_reopening_visible == 0
}