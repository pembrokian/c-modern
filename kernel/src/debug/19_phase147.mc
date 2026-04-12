import kv_service
import log_service
import shell_service
import syscall

func validate_phase147_ipc_shape_audit_under_real_usage(audit: Phase147IpcShapeAudit, scheduler_contract_hardened: u32, lifecycle_contract_hardened: u32, capability_contract_hardened: u32, ipc_contract_hardened: u32, address_space_contract_hardened: u32, interrupt_contract_hardened: u32, timer_contract_hardened: u32, barrier_contract_hardened: u32) bool {
    if !validate_phase146_service_shape_consolidation(audit.phase146, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened, barrier_contract_hardened) {
        return false
    }
    if audit.serial_service_pid == 0 || audit.shell_service_pid == 0 || audit.log_service_pid == 0 || audit.kv_service_pid == 0 {
        return false
    }
    if audit.serial_service_pid == audit.shell_service_pid || audit.serial_service_pid == audit.log_service_pid || audit.serial_service_pid == audit.kv_service_pid {
        return false
    }
    if audit.shell_service_pid == audit.log_service_pid || audit.shell_service_pid == audit.kv_service_pid || audit.log_service_pid == audit.kv_service_pid {
        return false
    }
    if audit.serial_observation.service_pid != audit.serial_service_pid {
        return false
    }
    if syscall.status_score(audit.serial_observation.forward_status) != 2 || syscall.status_score(audit.serial_observation.aggregate_reply_status) != 2 {
        return false
    }
    if audit.serial_observation.forwarded_request_count != 6 || audit.serial_observation.aggregate_reply_count != 6 {
        return false
    }
    if audit.serial_observation.forward_endpoint_id != audit.repeated_kv_get_route.endpoint_id {
        return false
    }
    if audit.serial_observation.forwarded_request_len != audit.repeated_kv_get_route.request_len {
        return false
    }
    if audit.serial_observation.forwarded_request_byte0 != audit.repeated_kv_get_route.request_byte0 || audit.serial_observation.forwarded_request_byte1 != audit.repeated_kv_get_route.request_byte1 {
        return false
    }
    if audit.serial_observation.aggregate_reply_len != audit.repeated_kv_get_route.reply_len {
        return false
    }
    if audit.serial_observation.aggregate_reply_byte0 != audit.repeated_kv_get_route.reply_byte0 || audit.serial_observation.aggregate_reply_byte1 != audit.repeated_kv_get_route.reply_byte1 || audit.serial_observation.aggregate_reply_byte2 != audit.repeated_kv_get_route.reply_byte2 || audit.serial_observation.aggregate_reply_byte3 != audit.repeated_kv_get_route.reply_byte3 {
        return false
    }
    if shell_service.tag_score(audit.log_append_route.tag) != 4 || shell_service.tag_score(audit.log_tail_route.tag) != 8 || shell_service.tag_score(audit.kv_set_route.tag) != 16 || shell_service.tag_score(audit.kv_get_route.tag) != 32 || shell_service.tag_score(audit.repeated_log_tail_route.tag) != 8 || shell_service.tag_score(audit.repeated_kv_get_route.tag) != 32 {
        return false
    }
    if audit.log_append_route.request_len != 3 || audit.log_append_route.request_byte0 != 76 || audit.log_append_route.request_byte1 != 65 || audit.log_append_route.request_byte2 != 76 {
        return false
    }
    if audit.log_append_route.reply_len != 2 || audit.log_append_route.reply_byte0 != 33 || audit.log_append_route.reply_byte1 != 0 {
        return false
    }
    if audit.log_tail_route.request_len != 2 || audit.log_tail_route.request_byte0 != 76 || audit.log_tail_route.request_byte1 != 84 {
        return false
    }
    if audit.log_tail_route.reply_len != 2 || audit.log_tail_route.reply_byte0 != 76 || audit.log_tail_route.reply_byte1 != 33 {
        return false
    }
    if audit.kv_set_route.request_len != 4 || audit.kv_set_route.request_byte0 != 75 || audit.kv_set_route.request_byte1 != 83 || audit.kv_set_route.request_byte2 != 81 || audit.kv_set_route.request_byte3 != 90 {
        return false
    }
    if audit.kv_set_route.reply_len != 4 || audit.kv_set_route.reply_byte0 != 81 || audit.kv_set_route.reply_byte1 != 90 || audit.kv_set_route.reply_byte2 != 0 || audit.kv_set_route.reply_byte3 != 0 {
        return false
    }
    if audit.kv_get_route.request_len != 4 || audit.kv_get_route.request_byte0 != 75 || audit.kv_get_route.request_byte1 != 71 || audit.kv_get_route.request_byte2 != 81 {
        return false
    }
    if audit.kv_get_route.reply_len != 4 || audit.kv_get_route.reply_byte0 != 81 || audit.kv_get_route.reply_byte1 != 90 || audit.kv_get_route.reply_byte2 != 1 || audit.kv_get_route.reply_byte3 != 0 {
        return false
    }
    if audit.repeated_log_tail_route.reply_len != 2 || audit.repeated_log_tail_route.reply_byte0 != 83 || audit.repeated_log_tail_route.reply_byte1 != 76 {
        return false
    }
    if audit.repeated_kv_get_route.reply_len != 4 || audit.repeated_kv_get_route.reply_byte0 != 81 || audit.repeated_kv_get_route.reply_byte1 != 90 || audit.repeated_kv_get_route.reply_byte2 != 1 || audit.repeated_kv_get_route.reply_byte3 != 0 {
        return false
    }
    if audit.repeated_log_tail_route.log_route_count != 3 || audit.repeated_log_tail_route.kv_route_count != 2 {
        return false
    }
    if audit.repeated_kv_get_route.log_route_count != 3 || audit.repeated_kv_get_route.kv_route_count != 3 {
        return false
    }
    if log_service.tag_score(audit.log_retention.tag) != 8 {
        return false
    }
    if audit.log_retention.retained_len != 2 || audit.log_retention.append_count != 2 || audit.log_retention.tail_query_count != 2 || audit.log_retention.overwrite_count != 0 {
        return false
    }
    if audit.log_retention.retained_byte0 != 76 || audit.log_retention.retained_byte1 != 83 || audit.log_retention.tail_byte0 != 83 || audit.log_retention.tail_byte1 != 76 {
        return false
    }
    if kv_service.tag_score(audit.kv_retention.tag) != 4 {
        return false
    }
    if audit.kv_retention.request_count != 3 || audit.kv_retention.set_count != 1 || audit.kv_retention.get_count != 2 || audit.kv_retention.overwrite_count != 0 || audit.kv_retention.missing_count != 0 || audit.kv_retention.unavailable_count != 0 {
        return false
    }
    if audit.kv_retention.retained_count != 1 || audit.kv_retention.found == 0 || audit.kv_retention.logged_write_count != 1 {
        return false
    }
    if audit.kv_retention.key_byte != 81 || audit.kv_retention.value_byte != 90 || audit.kv_retention.retained_key0 != 81 || audit.kv_retention.retained_value0 != 90 {
        return false
    }
    if audit.request_construction_sufficient_visible == 0 || audit.reply_shape_service_local_visible == 0 || audit.compact_encoding_sufficient_visible == 0 || audit.service_to_service_observation_visible == 0 {
        return false
    }
    if audit.generic_message_bus_visible != 0 || audit.dynamic_payload_typing_visible != 0 {
        return false
    }
    return audit.compiler_reopening_visible == 0
}