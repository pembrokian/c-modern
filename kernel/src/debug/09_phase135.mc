import interrupt
import syscall

func validate_phase135_buffer_ownership_boundary(audit: Phase135BufferOwnershipAudit, scheduler_contract_hardened: u32, lifecycle_contract_hardened: u32, capability_contract_hardened: u32, ipc_contract_hardened: u32, address_space_contract_hardened: u32, interrupt_contract_hardened: u32, timer_contract_hardened: u32, barrier_contract_hardened: u32) bool {
    if !validate_phase131_fan_out_composition(audit.phase131, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened, barrier_contract_hardened) {
        return false
    }
    if audit.interrupt_vector == 0 || audit.uart_service_endpoint_id == 0 {
        return false
    }
    if interrupt.dispatch_kind_score(audit.interrupt_kind) != 4 {
        return false
    }
    if audit.first_dispatch_handled != 1 || audit.second_dispatch_handled != 1 {
        return false
    }
    if audit.first_uart_ack_count != 1 || audit.first_uart_ingress_count != 1 || audit.first_uart_retire_count != 1 {
        return false
    }
    if audit.first_staged_payload_len != 2 || audit.first_published_payload_len != 2 {
        return false
    }
    if audit.first_staged_payload_byte0 != 70 || audit.first_staged_payload_byte1 != 82 {
        return false
    }
    if audit.first_published_endpoint_id != audit.uart_service_endpoint_id {
        return false
    }
    if audit.first_published_source_pid != 1 {
        return false
    }
    if audit.first_published_payload_byte0 != audit.first_staged_payload_byte0 || audit.first_published_payload_byte1 != audit.first_staged_payload_byte1 {
        return false
    }
    if audit.first_published_queued != 1 || audit.first_published_queue_full != 0 || audit.first_published_endpoint_valid != 1 || audit.first_published_endpoint_closed != 0 {
        return false
    }
    if audit.first_retired_payload_len != 0 || audit.first_retired_payload_byte0 != 0 || audit.first_retired_payload_byte1 != 0 {
        return false
    }
    if audit.second_uart_ack_count != 2 || audit.second_uart_ingress_count != 2 || audit.second_uart_retire_count != 2 {
        return false
    }
    if audit.second_staged_payload_len != 2 || audit.second_published_payload_len != 2 {
        return false
    }
    if audit.second_staged_payload_byte0 != 65 || audit.second_staged_payload_byte1 != 77 {
        return false
    }
    if audit.second_published_endpoint_id != audit.uart_service_endpoint_id {
        return false
    }
    if audit.second_published_source_pid != 1 {
        return false
    }
    if audit.second_published_payload_byte0 != audit.second_staged_payload_byte0 || audit.second_published_payload_byte1 != audit.second_staged_payload_byte1 {
        return false
    }
    if audit.second_published_queued != 1 || audit.second_published_queue_full != 0 || audit.second_published_endpoint_valid != 1 || audit.second_published_endpoint_closed != 0 {
        return false
    }
    if audit.second_retired_payload_len != 0 || audit.second_retired_payload_byte0 != 0 || audit.second_retired_payload_byte1 != 0 {
        return false
    }
    if audit.serial_service_pid == 0 {
        return false
    }
    if audit.serial_source_pid != 1 || audit.serial_endpoint_id != audit.uart_service_endpoint_id {
        return false
    }
    if syscall.status_score(audit.serial_receive_status) != 2 {
        return false
    }
    if syscall.status_score(audit.serial_wait_status) != 2 {
        return false
    }
    if audit.serial_last_payload_len != 2 || audit.serial_last_received_byte != 65 {
        return false
    }
    if audit.serial_ingress_count != 2 {
        return false
    }
    if audit.serial_log_len != 4 || audit.serial_total_consumed_bytes != 4 {
        return false
    }
    if audit.serial_log_byte0 != 70 || audit.serial_log_byte1 != 82 || audit.serial_log_byte2 != 65 || audit.serial_log_byte3 != 77 {
        return false
    }
    if audit.kernel_policy_visible != 0 {
        return false
    }
    if audit.driver_framework_visible != 0 {
        return false
    }
    if audit.generic_buffer_pool_visible != 0 {
        return false
    }
    if audit.zero_copy_visible != 0 {
        return false
    }
    return audit.compiler_reopening_visible == 0
}
