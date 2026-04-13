import serial_service
import syscall
import uart

func validate_phase136_device_failure_containment(audit: Phase136DeviceFailureContainmentAudit, scheduler_contract_hardened: u32, lifecycle_contract_hardened: u32, capability_contract_hardened: u32, ipc_contract_hardened: u32, address_space_contract_hardened: u32, interrupt_contract_hardened: u32, timer_contract_hardened: u32, barrier_contract_hardened: u32) bool {
    if !validate_phase131_fan_out_composition(audit.phase131, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened, barrier_contract_hardened) {
        return false
    }
    if audit.uart_service_endpoint_id == 0 {
        return false
    }
    if interrupt.dispatch_kind_score(audit.malformed_interrupt_kind) != 4 {
        return false
    }
    if audit.malformed_dispatch_handled != 1 {
        return false
    }
    if audit.malformed_published_queued != 1 || audit.malformed_published_queue_full != 0 || audit.malformed_published_endpoint_valid != 1 || audit.malformed_published_endpoint_closed != 0 {
        return false
    }
    if audit.malformed_serial_service_pid == 0 {
        return false
    }
    if serial_service.tag_score(audit.malformed_serial_tag) != 4 {
        return false
    }
    if syscall.status_score(audit.malformed_receive_status) != 2 {
        return false
    }
    if audit.malformed_payload_len != 1 || audit.malformed_received_byte != 255 {
        return false
    }
    if audit.malformed_ingress_count != 1 || audit.malformed_service_malformed_count != 1 {
        return false
    }
    if audit.malformed_log_len != 0 || audit.malformed_total_consumed_bytes != 0 {
        return false
    }
    if audit.queue_one_published_queued != 1 || audit.queue_two_published_queued != 1 {
        return false
    }
    if audit.queue_full_dispatch_handled != 1 {
        return false
    }
    if audit.queue_full_published_queued != 0 || audit.queue_full_published_queue_full != 1 || audit.queue_full_published_endpoint_valid != 1 || audit.queue_full_published_endpoint_closed != 0 {
        return false
    }
    if audit.queue_full_drop_count != 1 {
        return false
    }
    if uart.failure_kind_score(audit.queue_full_failure_kind) != 2 {
        return false
    }
    if audit.uart_ack_count_after_queue_full != 4 || audit.uart_ingress_count_after_queue_full != 4 || audit.uart_retire_count_after_queue_full != 4 {
        return false
    }
    if audit.close_endpoint_id != audit.uart_service_endpoint_id {
        return false
    }
    if audit.close_closed != 1 || audit.close_aborted_messages != 2 || audit.close_wake_count != 0 {
        return false
    }
    if syscall.status_score(audit.close_wait_status) != 2 {
        return false
    }
    if audit.closed_dispatch_handled != 1 {
        return false
    }
    if audit.closed_published_queued != 0 || audit.closed_published_queue_full != 0 || audit.closed_published_endpoint_valid != 1 || audit.closed_published_endpoint_closed != 1 {
        return false
    }
    if audit.closed_drop_count != 2 || audit.closed_endpoint_closed_drop_count != 1 {
        return false
    }
    if uart.failure_kind_score(audit.closed_failure_kind) != 4 {
        return false
    }
    if audit.kernel_policy_visible != 0 {
        return false
    }
    if audit.driver_framework_visible != 0 {
        return false
    }
    if audit.retry_framework_visible != 0 {
        return false
    }
    if audit.protocol_parsing_in_kernel_visible != 0 {
        return false
    }
    return audit.compiler_reopening_visible == 0
}
