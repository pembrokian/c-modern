func build_runtime_message(message_id: u32, source_pid: u32, endpoint_id: u32, payload_len: usize, payload: [4]u8, attached_count: usize, attached_endpoint_id: u32, attached_rights: u32, attached_source_handle_slot: u32) KernelMessage {
    if attached_count == 1 {
        return attached_message(message_id, source_pid, endpoint_id, payload_len, payload, attached_endpoint_id, attached_rights, attached_source_handle_slot)
    }
    return byte_message(message_id, source_pid, endpoint_id, payload_len, payload)
}

func enqueue_runtime_message(table: EndpointTable, endpoint_id: u32, message: KernelMessage) RuntimeSendResult {
    dispatch: EndpointDispatch = resolve_runtime_endpoint(table, endpoint_id)
    if dispatch.valid == 0 {
        return RuntimeSendResult{ endpoints: table, endpoint_id: endpoint_id, queued: 0, queue_full: 0, endpoint_valid: 0 }
    }
    updated_endpoints: EndpointTable = enqueue_message(table, dispatch.endpoint_index, message)
    if !enqueue_succeeded(table, updated_endpoints, dispatch.endpoint_index) {
        return RuntimeSendResult{ endpoints: table, endpoint_id: endpoint_id, queued: 0, queue_full: 1, endpoint_valid: 1 }
    }
    return RuntimeSendResult{ endpoints: updated_endpoints, endpoint_id: endpoint_id, queued: 1, queue_full: 0, endpoint_valid: 1 }
}

func receive_runtime_message(table: EndpointTable, endpoint_id: u32) RuntimeReceiveResult {
    dispatch: EndpointDispatch = resolve_runtime_endpoint(table, endpoint_id)
    if dispatch.valid == 0 {
        return RuntimeReceiveResult{ endpoints: table, endpoint_id: endpoint_id, message: empty_message(), available: 0, queue_empty: 0, endpoint_valid: 0 }
    }
    message: KernelMessage = peek_head_message(table, dispatch.endpoint_index)
    if message_state_score(message.state) == 1 {
        return RuntimeReceiveResult{ endpoints: table, endpoint_id: endpoint_id, message: empty_message(), available: 0, queue_empty: 1, endpoint_valid: 1 }
    }
    updated_endpoints: EndpointTable = consume_head_message(table, dispatch.endpoint_index)
    return RuntimeReceiveResult{ endpoints: updated_endpoints, endpoint_id: endpoint_id, message: message, available: 1, queue_empty: 0, endpoint_valid: 1 }
}