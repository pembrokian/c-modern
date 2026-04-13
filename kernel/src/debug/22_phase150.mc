import kv_service
import log_service
import shell_service
import syscall

func validate_phase150_one_system_rebuilt_cleanly(audit: Phase150OneSystemRebuiltCleanlyAudit, scheduler_contract_hardened: u32, lifecycle_contract_hardened: u32, capability_contract_hardened: u32, ipc_contract_hardened: u32, address_space_contract_hardened: u32, interrupt_contract_hardened: u32, timer_contract_hardened: u32, barrier_contract_hardened: u32) bool {
    if !validate_phase149_restart_semantics_first_class_pattern(audit.phase149, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened, barrier_contract_hardened) {
        return false
    }
    if audit.serial_service_pid == 0 || audit.shell_service_pid == 0 || audit.log_service_pid == 0 || audit.kv_service_pid == 0 {
        return false
    }
    if audit.serial_service_pid != audit.phase149.phase148.phase147.serial_service_pid {
        return false
    }
    if audit.shell_service_pid != audit.phase149.shell_service_pid || audit.log_service_pid != audit.phase149.log_service_pid || audit.kv_service_pid != audit.phase149.kv_service_pid {
        return false
    }
    if shell_service.tag_score(audit.log_append_route.tag) != shell_service.tag_score(audit.phase149.phase148.phase147.log_append_route.tag) {
        return false
    }
    if shell_service.tag_score(audit.log_tail_route.tag) != shell_service.tag_score(audit.phase149.phase148.phase147.log_tail_route.tag) {
        return false
    }
    if audit.log_append_route.request_len != audit.phase149.phase148.phase147.log_append_route.request_len || audit.log_append_route.request_byte0 != audit.phase149.phase148.phase147.log_append_route.request_byte0 || audit.log_append_route.request_byte1 != audit.phase149.phase148.phase147.log_append_route.request_byte1 || audit.log_append_route.request_byte2 != audit.phase149.phase148.phase147.log_append_route.request_byte2 {
        return false
    }
    if audit.log_append_route.reply_len != audit.phase149.phase148.phase147.log_append_route.reply_len || audit.log_append_route.reply_byte0 != audit.phase149.phase148.phase147.log_append_route.reply_byte0 || audit.log_append_route.reply_byte1 != audit.phase149.phase148.phase147.log_append_route.reply_byte1 {
        return false
    }
    if audit.log_tail_route.request_len != audit.phase149.phase148.phase147.log_tail_route.request_len || audit.log_tail_route.request_byte0 != audit.phase149.phase148.phase147.log_tail_route.request_byte0 || audit.log_tail_route.request_byte1 != audit.phase149.phase148.phase147.log_tail_route.request_byte1 {
        return false
    }
    if audit.log_tail_route.reply_len != audit.phase149.phase148.phase147.log_tail_route.reply_len || audit.log_tail_route.reply_byte0 != audit.phase149.phase148.phase147.log_tail_route.reply_byte0 || audit.log_tail_route.reply_byte1 != audit.phase149.phase148.phase147.log_tail_route.reply_byte1 {
        return false
    }
    if shell_service.tag_score(audit.pre_failure_set_route.tag) != shell_service.tag_score(audit.phase149.phase148.phase147.phase146.phase145.pre_failure_set_route.tag) {
        return false
    }
    if shell_service.tag_score(audit.pre_failure_get_route.tag) != shell_service.tag_score(audit.phase149.pre_failure_get_route.tag) || shell_service.tag_score(audit.failed_get_route.tag) != shell_service.tag_score(audit.phase149.failed_get_route.tag) || shell_service.tag_score(audit.restarted_get_route.tag) != shell_service.tag_score(audit.phase149.restarted_get_route.tag) {
        return false
    }
    if audit.pre_failure_set_route.request_byte0 != audit.phase149.phase148.phase147.phase146.phase145.pre_failure_set_route.request_byte0 || audit.pre_failure_set_route.request_byte1 != audit.phase149.phase148.phase147.phase146.phase145.pre_failure_set_route.request_byte1 || audit.pre_failure_set_route.request_byte2 != audit.phase149.phase148.phase147.phase146.phase145.pre_failure_set_route.request_byte2 || audit.pre_failure_set_route.request_byte3 != audit.phase149.phase148.phase147.phase146.phase145.pre_failure_set_route.request_byte3 {
        return false
    }
    if audit.pre_failure_set_route.reply_byte0 != audit.phase149.phase148.phase147.phase146.phase145.pre_failure_set_route.reply_byte0 || audit.pre_failure_set_route.reply_byte1 != audit.phase149.phase148.phase147.phase146.phase145.pre_failure_set_route.reply_byte1 || audit.pre_failure_set_route.reply_byte2 != audit.phase149.phase148.phase147.phase146.phase145.pre_failure_set_route.reply_byte2 || audit.pre_failure_set_route.reply_byte3 != audit.phase149.phase148.phase147.phase146.phase145.pre_failure_set_route.reply_byte3 {
        return false
    }
    if audit.pre_failure_get_route.request_byte2 != audit.phase149.pre_failure_get_route.request_byte2 || audit.failed_get_route.request_byte2 != audit.phase149.failed_get_route.request_byte2 || audit.restarted_get_route.request_byte2 != audit.phase149.restarted_get_route.request_byte2 {
        return false
    }
    if audit.pre_failure_get_route.reply_byte0 != audit.phase149.pre_failure_get_route.reply_byte0 || audit.pre_failure_get_route.reply_byte1 != audit.phase149.pre_failure_get_route.reply_byte1 || audit.pre_failure_get_route.reply_byte2 != audit.phase149.pre_failure_get_route.reply_byte2 || audit.pre_failure_get_route.reply_byte3 != audit.phase149.pre_failure_get_route.reply_byte3 {
        return false
    }
    if audit.failed_get_route.reply_byte0 != audit.phase149.failed_get_route.reply_byte0 || audit.failed_get_route.reply_byte1 != audit.phase149.failed_get_route.reply_byte1 || audit.failed_get_route.reply_byte2 != audit.phase149.failed_get_route.reply_byte2 || audit.failed_get_route.reply_byte3 != audit.phase149.failed_get_route.reply_byte3 {
        return false
    }
    if audit.restarted_get_route.reply_byte0 != audit.phase149.restarted_get_route.reply_byte0 || audit.restarted_get_route.reply_byte1 != audit.phase149.restarted_get_route.reply_byte1 || audit.restarted_get_route.reply_byte2 != audit.phase149.restarted_get_route.reply_byte2 || audit.restarted_get_route.reply_byte3 != audit.phase149.restarted_get_route.reply_byte3 {
        return false
    }
    if kv_service.tag_score(audit.pre_failure_retention.tag) != kv_service.tag_score(audit.phase149.phase148.phase147.phase146.phase145.pre_failure_retention.tag) {
        return false
    }
    if audit.pre_failure_retention.available == 0 || audit.pre_failure_retention.retained_count != 1 || audit.pre_failure_retention.key_byte != 75 || audit.pre_failure_retention.value_byte != 86 || audit.pre_failure_retention.retained_key0 != 75 || audit.pre_failure_retention.retained_value0 != 86 {
        return false
    }
    if kv_service.tag_score(audit.post_restart_retention.tag) != kv_service.tag_score(audit.phase149.post_restart_retention.tag) {
        return false
    }
    if audit.post_restart_retention.available == 0 || audit.post_restart_retention.retained_count != 0 || audit.post_restart_retention.get_count != 1 || audit.post_restart_retention.missing_count != 1 {
        return false
    }
    if audit.restart.owner_pid != audit.phase149.restart.owner_pid || audit.restart.service_key != audit.phase149.restart.service_key || audit.restart.shared_control_endpoint_id != audit.phase149.restart.shared_control_endpoint_id {
        return false
    }
    if log_service.tag_score(audit.event_log_retention.tag) != 16 {
        return false
    }
    if audit.event_log_retention.retained_len != 4 || audit.event_log_retention.append_count != 4 || audit.event_log_retention.tail_query_count != 1 || audit.event_log_retention.overwrite_count != 0 {
        return false
    }
    if audit.event_log_retention.retained_byte0 != 76 || audit.event_log_retention.retained_byte1 != 83 || audit.event_log_retention.retained_byte2 != 70 || audit.event_log_retention.retained_byte3 != 82 {
        return false
    }
    if audit.event_log_retention.tail_byte0 != 82 || audit.event_log_retention.tail_byte1 != 70 {
        return false
    }
    if audit.rebuilt_system_helper_visible == 0 || audit.owner_local_system_state_visible == 0 || audit.manual_state_threading_removed_visible == 0 {
        return false
    }
    if audit.dynamic_service_framework_visible != 0 {
        return false
    }
    return audit.compiler_reopening_visible == 0
}