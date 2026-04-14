import syscall

func validate_phase140_serial_ingress_composed_service_graph(audit: Phase140SerialIngressComposedServiceGraphAudit, scheduler_contract_hardened: u32, lifecycle_contract_hardened: u32, capability_contract_hardened: u32, ipc_contract_hardened: u32, address_space_contract_hardened: u32, interrupt_contract_hardened: u32, timer_contract_hardened: u32, barrier_contract_hardened: u32) bool {
    if !validate_phase137_optional_dma_or_equivalent_follow_through(audit.phase137, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened, barrier_contract_hardened) {
        return false
    }
    if audit.serial_service_pid == 0 || audit.composition_service_pid == 0 {
        return false
    }
    if audit.serial_service_pid == audit.composition_service_pid {
        return false
    }
    if audit.serial_forward_endpoint_id == 0 || audit.serial_forward_endpoint_id != audit.composition_control_endpoint_id {
        return false
    }
    if syscall.status_score(audit.serial_forward_status) != 2 {
        return false
    }
    if audit.serial_forward_request_len != 2 || audit.serial_forward_count != 1 {
        return false
    }
    if audit.serial_forward_request_byte0 != audit.phase137.serial_log_byte0 || audit.serial_forward_request_byte1 != audit.phase137.serial_log_byte1 {
        return false
    }
    if syscall.status_score(audit.composition_request_receive_status) != 2 {
        return false
    }
    if audit.composition_request_source_pid != audit.serial_service_pid {
        return false
    }
    if audit.composition_request_len != audit.serial_forward_request_len {
        return false
    }
    if audit.composition_request_byte0 != audit.serial_forward_request_byte0 || audit.composition_request_byte1 != audit.serial_forward_request_byte1 {
        return false
    }
    if audit.composition_control_endpoint_id == 0 || audit.composition_echo_endpoint_id == 0 || audit.composition_log_endpoint_id == 0 {
        return false
    }
    if audit.composition_control_endpoint_id == audit.composition_echo_endpoint_id || audit.composition_control_endpoint_id == audit.composition_log_endpoint_id {
        return false
    }
    if audit.composition_echo_endpoint_id == audit.composition_log_endpoint_id {
        return false
    }
    if syscall.status_score(audit.composition_echo_fanout_status) != 2 || syscall.status_score(audit.composition_log_fanout_status) != 2 {
        return false
    }
    if audit.composition_echo_fanout_endpoint_id != audit.composition_echo_endpoint_id {
        return false
    }
    if audit.composition_log_fanout_endpoint_id != audit.composition_log_endpoint_id {
        return false
    }
    if syscall.status_score(audit.composition_aggregate_reply_status) != 2 {
        return false
    }
    if audit.composition_outbound_edge_count != 2 || audit.composition_aggregate_reply_count != 1 {
        return false
    }
    if syscall.status_score(audit.serial_aggregate_reply_status) != 2 {
        return false
    }
    if audit.serial_aggregate_reply_len != 4 || audit.serial_aggregate_reply_count != 1 {
        return false
    }
    if audit.serial_aggregate_reply_byte0 != audit.serial_forward_request_byte0 || audit.serial_aggregate_reply_byte1 != audit.serial_forward_request_byte1 {
        return false
    }
    if audit.serial_aggregate_reply_byte2 != audit.phase137.phase136.phase131.phase130.phase129.phase128.phase126.phase125.phase124.phase123.phase122.phase121.phase120.phase119.phase118.phase117.log_service_handshake.ack_byte {
        return false
    }
    if audit.serial_aggregate_reply_byte3 != 2 {
        return false
    }
    if audit.kernel_broker_visible != 0 {
        return false
    }
    if audit.dynamic_routing_visible != 0 {
        return false
    }
    if audit.general_service_graph_visible != 0 {
        return false
    }
    return audit.compiler_reopening_visible == 0
}