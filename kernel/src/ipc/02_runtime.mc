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

func register_runtime_blocked_sender(table: EndpointTable, endpoint_id: u32, task_slot: u32, task_id: u32) EndpointTable {
    dispatch: EndpointDispatch = resolve_runtime_endpoint(table, endpoint_id)
    if dispatch.valid == 0 {
        return table
    }
    return register_blocked_sender(table, dispatch.endpoint_index, task_slot, task_id)
}

func register_runtime_blocked_receiver(table: EndpointTable, endpoint_id: u32, task_slot: u32, task_id: u32) EndpointTable {
    dispatch: EndpointDispatch = resolve_runtime_endpoint(table, endpoint_id)
    if dispatch.valid == 0 {
        return table
    }
    return register_blocked_receiver(table, dispatch.endpoint_index, task_slot, task_id)
}

func wake_runtime_blocked_sender(table: EndpointTable, endpoint_id: u32, wake_reason: EndpointWakeReason) EndpointWakeTransition {
    dispatch: EndpointDispatch = resolve_runtime_endpoint(table, endpoint_id)
    if dispatch.valid == 0 {
        return EndpointWakeTransition{ endpoints: table, wake: no_wake() }
    }
    return take_blocked_sender(table, dispatch.endpoint_index, wake_reason)
}

func wake_runtime_blocked_receiver(table: EndpointTable, endpoint_id: u32, wake_reason: EndpointWakeReason) EndpointWakeTransition {
    dispatch: EndpointDispatch = resolve_runtime_endpoint(table, endpoint_id)
    if dispatch.valid == 0 {
        return EndpointWakeTransition{ endpoints: table, wake: no_wake() }
    }
    return take_blocked_receiver(table, dispatch.endpoint_index, wake_reason)
}

func attempt_runtime_send(table: EndpointTable, endpoint_id: u32, message: KernelMessage) RuntimeSendTransition {
    lookup: EndpointLookup = lookup_runtime_endpoint(table, endpoint_id)
    if lookup.found == 0 {
        return RuntimeSendTransition{ endpoints: table, endpoint_id: endpoint_id, wake: no_wake(), queued: 0, queue_full: 0, endpoint_valid: 0, endpoint_closed: 0 }
    }
    if lookup.active == 0 {
        return RuntimeSendTransition{ endpoints: table, endpoint_id: endpoint_id, wake: no_wake(), queued: 0, queue_full: 0, endpoint_valid: 1, endpoint_closed: 1 }
    }
    updated_endpoints: EndpointTable = enqueue_message(table, lookup.endpoint_index, message)
    if !enqueue_succeeded(table, updated_endpoints, lookup.endpoint_index) {
        return RuntimeSendTransition{ endpoints: table, endpoint_id: endpoint_id, wake: no_wake(), queued: 0, queue_full: 1, endpoint_valid: 1, endpoint_closed: 0 }
    }
    wake_transition: EndpointWakeTransition = take_blocked_receiver(updated_endpoints, lookup.endpoint_index, EndpointWakeReason.MessageAvailable)
    return RuntimeSendTransition{ endpoints: wake_transition.endpoints, endpoint_id: endpoint_id, wake: wake_transition.wake, queued: 1, queue_full: 0, endpoint_valid: 1, endpoint_closed: 0 }
}

func attempt_runtime_receive(table: EndpointTable, endpoint_id: u32) RuntimeReceiveTransition {
    lookup: EndpointLookup = lookup_runtime_endpoint(table, endpoint_id)
    if lookup.found == 0 {
        return RuntimeReceiveTransition{ endpoints: table, endpoint_id: endpoint_id, message: empty_message(), wake: no_wake(), available: 0, queue_empty: 0, endpoint_valid: 0, endpoint_closed: 0 }
    }
    if lookup.active == 0 {
        return RuntimeReceiveTransition{ endpoints: table, endpoint_id: endpoint_id, message: empty_message(), wake: no_wake(), available: 0, queue_empty: 0, endpoint_valid: 1, endpoint_closed: 1 }
    }
    message: KernelMessage = peek_head_message(table, lookup.endpoint_index)
    if message_state_score(message.state) == 1 {
        return RuntimeReceiveTransition{ endpoints: table, endpoint_id: endpoint_id, message: empty_message(), wake: no_wake(), available: 0, queue_empty: 1, endpoint_valid: 1, endpoint_closed: 0 }
    }
    updated_endpoints: EndpointTable = consume_head_message(table, lookup.endpoint_index)
    wake_transition: EndpointWakeTransition = take_blocked_sender(updated_endpoints, lookup.endpoint_index, EndpointWakeReason.QueueSpaceAvailable)
    return RuntimeReceiveTransition{ endpoints: wake_transition.endpoints, endpoint_id: endpoint_id, message: message, wake: wake_transition.wake, available: 1, queue_empty: 0, endpoint_valid: 1, endpoint_closed: 0 }
}

func observe_runtime_publish(endpoint_id: u32, source_pid: u32, payload_len: usize, payload: [4]u8, transition: RuntimeSendTransition) RuntimePublishObservation {
    payload0: u8 = 0
    if payload_len != 0 {
        payload0 = payload[0]
    }
    return RuntimePublishObservation{ endpoint_id: endpoint_id, source_pid: source_pid, payload_len: payload_len, payload0: payload0, queued: transition.queued, queue_full: transition.queue_full, endpoint_valid: transition.endpoint_valid, endpoint_closed: transition.endpoint_closed }
}

func publish_runtime_byte(table: EndpointTable, endpoint_id: u32, source_pid: u32, message_id: u32, payload0: u8) RuntimePublishResult {
    payload: [4]u8 = zero_payload()
    payload[0] = payload0
    transition: RuntimeSendTransition = attempt_runtime_send(table, endpoint_id, byte_message(message_id, source_pid, endpoint_id, 1, payload))
    return RuntimePublishResult{ endpoints: transition.endpoints, wake: transition.wake, observation: observe_runtime_publish(endpoint_id, source_pid, 1, payload, transition) }
}