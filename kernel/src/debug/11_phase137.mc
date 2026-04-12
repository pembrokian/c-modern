import interrupt
import serial_service
import syscall

func validate_phase137_optional_dma_or_equivalent_follow_through(audit: Phase137OptionalDmaOrEquivalentAudit, scheduler_contract_hardened: u32, lifecycle_contract_hardened: u32, capability_contract_hardened: u32, ipc_contract_hardened: u32, address_space_contract_hardened: u32, interrupt_contract_hardened: u32, timer_contract_hardened: u32, barrier_contract_hardened: u32) bool {
    if !validate_phase136_device_failure_containment(audit.phase136, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened, barrier_contract_hardened) {
        return false
    }
    if interrupt.dispatch_kind_score(audit.completion_interrupt_kind) != 8 {
        return false
    }
    if audit.completion_dispatch_handled != 1 {
        return false
    }
    if audit.completion_endpoint_id == 0 {
        return false
    }
    if audit.completion_staged_payload_len != 4 || audit.completion_published_payload_len != 4 {
        return false
    }
    if audit.completion_staged_payload0 != 68 || audit.completion_staged_payload1 != 77 || audit.completion_staged_payload2 != 65 || audit.completion_staged_payload3 != 33 {
        return false
    }
    if audit.completion_published_payload0 != audit.completion_staged_payload0 || audit.completion_published_payload1 != audit.completion_staged_payload1 || audit.completion_published_payload2 != audit.completion_staged_payload2 || audit.completion_published_payload3 != audit.completion_staged_payload3 {
        return false
    }
    if audit.completion_retired_payload_len != 0 || audit.completion_retired_payload0 != 0 || audit.completion_retired_payload1 != 0 || audit.completion_retired_payload2 != 0 || audit.completion_retired_payload3 != 0 {
        return false
    }
    if audit.completion_publish_queued != 1 || audit.completion_publish_queue_full != 0 || audit.completion_publish_endpoint_valid != 1 || audit.completion_publish_endpoint_closed != 0 {
        return false
    }
    if audit.completion_ingress_count != 1 || audit.completion_retire_count != 1 {
        return false
    }
    if audit.serial_service_pid == 0 {
        return false
    }
    if syscall.status_score(audit.serial_receive_status) != 2 {
        return false
    }
    if serial_service.tag_score(audit.serial_tag) != 2 {
        return false
    }
    if audit.serial_payload_len != 4 || audit.serial_received_byte != 68 {
        return false
    }
    if audit.serial_ingress_count != 1 {
        return false
    }
    if audit.serial_log_len != 4 || audit.serial_total_consumed_bytes != 4 {
        return false
    }
    if audit.serial_log_byte0 != 68 || audit.serial_log_byte1 != 77 || audit.serial_log_byte2 != 65 || audit.serial_log_byte3 != 33 {
        return false
    }
    if audit.dma_manager_visible != 0 {
        return false
    }
    if audit.descriptor_framework_visible != 0 {
        return false
    }
    return audit.compiler_reopening_visible == 0
}
