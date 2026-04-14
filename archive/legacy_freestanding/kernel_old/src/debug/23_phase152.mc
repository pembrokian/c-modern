import kv_service
import log_service
import shell_service

func validate_phase152_pattern_stability(audit: Phase152PatternStabilityAudit, scheduler_contract_hardened: u32, lifecycle_contract_hardened: u32, capability_contract_hardened: u32, ipc_contract_hardened: u32, address_space_contract_hardened: u32, interrupt_contract_hardened: u32, timer_contract_hardened: u32, barrier_contract_hardened: u32) bool {
    if !validate_phase150_one_system_rebuilt_cleanly(audit.phase150, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened, barrier_contract_hardened) {
        return false
    }
    if audit.serial_service_pid == 0 || audit.shell_service_pid == 0 || audit.log_service_pid == 0 || audit.kv_service_pid == 0 {
        return false
    }
    if audit.serial_service_pid != audit.phase150.serial_service_pid || audit.shell_service_pid != audit.phase150.shell_service_pid || audit.log_service_pid != audit.phase150.log_service_pid || audit.kv_service_pid != audit.phase150.kv_service_pid {
        return false
    }
    if shell_service.tag_score(audit.pre_restart_log_append_route.tag) != shell_service.tag_score(audit.phase150.log_append_route.tag) {
        return false
    }
    if shell_service.tag_score(audit.pre_restart_log_tail_route.tag) != shell_service.tag_score(audit.phase150.log_tail_route.tag) {
        return false
    }
    if shell_service.tag_score(audit.pre_restart_kv_set_route.tag) != shell_service.tag_score(audit.phase150.pre_failure_set_route.tag) {
        return false
    }
    if shell_service.tag_score(audit.pre_restart_kv_get_route.tag) != shell_service.tag_score(audit.phase150.pre_failure_get_route.tag) {
        return false
    }
    if shell_service.tag_score(audit.post_restart_log_tail_route.tag) != shell_service.tag_score(audit.phase150.log_tail_route.tag) {
        return false
    }
    if shell_service.tag_score(audit.post_restart_kv_get_route.tag) != shell_service.tag_score(audit.phase150.pre_failure_get_route.tag) {
        return false
    }
    if audit.pre_restart_log_append_route.request_byte0 != audit.phase150.log_append_route.request_byte0 || audit.pre_restart_log_append_route.request_byte1 != audit.phase150.log_append_route.request_byte1 || audit.pre_restart_log_append_route.reply_len != audit.phase150.log_append_route.reply_len || audit.pre_restart_log_append_route.reply_byte0 != audit.phase150.log_append_route.reply_byte0 {
        return false
    }
    if audit.pre_restart_log_tail_route.request_byte0 != audit.phase150.log_tail_route.request_byte0 || audit.pre_restart_log_tail_route.request_byte1 != audit.phase150.log_tail_route.request_byte1 || audit.pre_restart_log_tail_route.reply_len != audit.phase150.log_tail_route.reply_len || audit.pre_restart_log_tail_route.reply_byte0 != 86 || audit.pre_restart_log_tail_route.reply_byte1 != 33 {
        return false
    }
    if audit.pre_restart_kv_set_route.request_byte0 != audit.phase150.pre_failure_set_route.request_byte0 || audit.pre_restart_kv_set_route.request_byte1 != audit.phase150.pre_failure_set_route.request_byte1 || audit.pre_restart_kv_set_route.request_byte2 != audit.phase150.pre_failure_set_route.request_byte2 || audit.pre_restart_kv_set_route.request_byte3 != audit.phase150.pre_failure_set_route.request_byte3 {
        return false
    }
    if audit.pre_restart_kv_get_route.request_byte0 != audit.phase150.pre_failure_get_route.request_byte0 || audit.pre_restart_kv_get_route.request_byte1 != audit.phase150.pre_failure_get_route.request_byte1 || audit.pre_restart_kv_get_route.request_byte2 != audit.phase150.pre_failure_get_route.request_byte2 {
        return false
    }
    if audit.pre_restart_kv_get_route.reply_byte0 != audit.phase150.pre_failure_get_route.reply_byte0 || audit.pre_restart_kv_get_route.reply_byte1 != audit.phase150.pre_failure_get_route.reply_byte1 || audit.pre_restart_kv_get_route.reply_byte2 != audit.phase150.pre_failure_get_route.reply_byte2 || audit.pre_restart_kv_get_route.reply_byte3 != audit.phase150.pre_failure_get_route.reply_byte3 {
        return false
    }
    if audit.post_restart_log_tail_route.reply_byte0 != 0 || audit.post_restart_log_tail_route.reply_byte1 != 33 {
        return false
    }
    if audit.post_restart_kv_get_route.reply_byte0 != audit.pre_restart_kv_get_route.reply_byte0 || audit.post_restart_kv_get_route.reply_byte1 != audit.pre_restart_kv_get_route.reply_byte1 || audit.post_restart_kv_get_route.reply_byte2 != audit.pre_restart_kv_get_route.reply_byte2 || audit.post_restart_kv_get_route.reply_byte3 != audit.pre_restart_kv_get_route.reply_byte3 {
        return false
    }
    if log_service.tag_score(audit.pre_restart_log_retention.tag) != 16 {
        return false
    }
    if audit.pre_restart_log_retention.retained_len != 2 || audit.pre_restart_log_retention.append_count != 2 || audit.pre_restart_log_retention.tail_query_count != 1 || audit.pre_restart_log_retention.retained_byte0 != 86 || audit.pre_restart_log_retention.retained_byte1 != 83 || audit.pre_restart_log_retention.tail_byte0 != 83 || audit.pre_restart_log_retention.tail_byte1 != 86 {
        return false
    }
    if log_service.tag_score(audit.post_restart_log_retention.tag) != 8 {
        return false
    }
    if audit.post_restart_log_retention.retained_len != 0 || audit.post_restart_log_retention.append_count != 0 || audit.post_restart_log_retention.tail_query_count != 1 || audit.post_restart_log_retention.retained_byte0 != 0 || audit.post_restart_log_retention.tail_byte0 != 0 || audit.post_restart_log_retention.tail_byte1 != 33 {
        return false
    }
    if kv_service.tag_score(audit.post_restart_kv_retention.tag) != kv_service.tag_score(audit.phase150.pre_failure_retention.tag) {
        return false
    }
    if audit.post_restart_kv_retention.available == 0 || audit.post_restart_kv_retention.retained_count != 1 || audit.post_restart_kv_retention.key_byte != 75 || audit.post_restart_kv_retention.value_byte != 86 || audit.post_restart_kv_retention.retained_key0 != 75 || audit.post_restart_kv_retention.retained_value0 != 86 {
        return false
    }
    if audit.restart.owner_pid != audit.phase150.restart.owner_pid || audit.restart.shared_control_endpoint_id != audit.phase150.restart.shared_control_endpoint_id || audit.restart.service_key != 1 || audit.restart.restart_count != 1 || audit.restart.retained_boot_wiring_visible == 0 {
        return false
    }
    if audit.rebuilt_system_helper_survives_visible == 0 || audit.shell_command_shape_survives_visible == 0 || audit.unaffected_peer_state_survives_visible == 0 || audit.restarted_service_state_resets_visible == 0 || audit.restart_evidence_moves_outside_restarted_owner_visible == 0 || audit.partial_generalization_visible == 0 {
        return false
    }
    if audit.full_generalization_visible != 0 || audit.pattern_failure_visible != 0 || audit.dynamic_service_framework_visible != 0 {
        return false
    }
    return audit.compiler_reopening_visible == 0
}