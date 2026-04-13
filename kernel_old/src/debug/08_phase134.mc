import state
import syscall

func validate_phase134_minimal_device_service_handoff(audit: Phase134MinimalDeviceServiceHandoffAudit, scheduler_contract_hardened: u32, lifecycle_contract_hardened: u32, capability_contract_hardened: u32, ipc_contract_hardened: u32, address_space_contract_hardened: u32, interrupt_contract_hardened: u32, timer_contract_hardened: u32, barrier_contract_hardened: u32) bool {
    if !validate_phase131_fan_out_composition(audit.phase131, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened, barrier_contract_hardened) {
        return false
    }
    if audit.interrupt_vector == 0 || audit.uart_service_endpoint_id == 0 {
        return false
    }
    if interrupt.dispatch_kind_score(audit.interrupt_kind) != 4 {
        return false
    }
    if audit.dispatch_handled != 1 {
        return false
    }
    if audit.uart_ack_count != 1 || audit.uart_ingress_count != 1 {
        return false
    }
    if audit.published_endpoint_id != audit.uart_service_endpoint_id {
        return false
    }
    if audit.published_source_pid != 1 {
        return false
    }
    if audit.published_payload_len != 1 {
        return false
    }
    if audit.published_payload_byte0 != audit.uart_received_byte {
        return false
    }
    if audit.published_queued != 1 {
        return false
    }
    if audit.published_queue_full != 0 || audit.published_endpoint_valid != 1 || audit.published_endpoint_closed != 0 {
        return false
    }
    if audit.serial_service_pid == 0 {
        return false
    }
    if syscall.status_score(audit.serial_receive_status) != 2 {
        return false
    }
    if syscall.status_score(audit.serial_wait_status) != 2 {
        return false
    }
    if audit.serial_received_byte != audit.uart_received_byte {
        return false
    }
    if audit.serial_ingress_count != 1 {
        return false
    }
    if audit.kernel_policy_visible != 0 {
        return false
    }
    if audit.driver_framework_visible != 0 {
        return false
    }
    if audit.generic_event_bus_visible != 0 {
        return false
    }
    return audit.compiler_reopening_visible == 0
}