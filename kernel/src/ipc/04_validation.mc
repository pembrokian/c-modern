func validate_syscall_ipc_boundary() bool {
    table: EndpointTable = empty_table()
    table = install_endpoint(table, 2, 9)
    payload: [4]u8 = zero_payload()
    payload[0] = 83
    payload[1] = 89
    payload[2] = 83
    payload[3] = 67
    message: KernelMessage = build_runtime_message(5, 2, 9, 4, payload, 0, 0, 0, 0)
    sent: RuntimeSendResult = enqueue_runtime_message(table, 9, message)
    if sent.endpoint_valid == 0 || sent.queued == 0 {
        return false
    }
    received: RuntimeReceiveResult = receive_runtime_message(sent.endpoints, 9)
    if received.endpoint_valid == 0 || received.available == 0 {
        return false
    }
    if received.message.source_pid != 2 {
        return false
    }
    if received.message.payload[0] != 83 {
        return false
    }
    empty_receive: RuntimeReceiveResult = receive_runtime_message(received.endpoints, 9)
    if empty_receive.queue_empty == 0 {
        return false
    }

    blocked_sender: EndpointTable = register_runtime_blocked_sender(empty_receive.endpoints, 9, 1, 21)
    sender_wake: EndpointWakeTransition = wake_runtime_blocked_sender(blocked_sender, 9, EndpointWakeReason.QueueSpaceAvailable)
    if sender_wake.wake.woke == 0 {
        return false
    }
    if sender_wake.wake.task_slot != 1 {
        return false
    }
    if sender_wake.wake.task_id != 21 {
        return false
    }
    if wake_reason_score(sender_wake.wake.wake_reason) != 2 {
        return false
    }
    if sender_wake.endpoints.slots[0].blocked_sender.waiting != 0 {
        return false
    }

    blocked_receiver: EndpointTable = register_runtime_blocked_receiver(sender_wake.endpoints, 9, 2, 22)
    receiver_wake: EndpointWakeTransition = wake_runtime_blocked_receiver(blocked_receiver, 9, EndpointWakeReason.MessageAvailable)
    if receiver_wake.wake.woke == 0 {
        return false
    }
    if receiver_wake.wake.task_slot != 2 {
        return false
    }
    if receiver_wake.wake.task_id != 22 {
        return false
    }
    if wake_reason_score(receiver_wake.wake.wake_reason) != 4 {
        return false
    }
    if receiver_wake.endpoints.slots[0].blocked_receiver.waiting != 0 {
        return false
    }

    active_table: EndpointTable = install_endpoint(empty_table(), 7, 15)
    close_payload: [4]u8 = zero_payload()
    close_payload[0] = 67
    close_payload[1] = 76
    close_payload[2] = 79
    close_payload[3] = 83
    close_message: KernelMessage = build_runtime_message(8, 7, 15, 4, close_payload, 0, 0, 0, 0)
    delivered: RuntimeSendTransition = attempt_runtime_send(active_table, 15, close_message)
    if delivered.queued == 0 {
        return false
    }
    blocked_close: EndpointTable = register_runtime_blocked_receiver(delivered.endpoints, 15, 2, 33)
    closed: EndpointCloseTransition = close_runtime_endpoint(blocked_close, 15)
    if closed.endpoint_found == 0 || closed.closed == 0 {
        return false
    }
    if closed.aborted_messages != 1 {
        return false
    }
    if closed.wakes.count != 1 {
        return false
    }
    if wake_reason_score(closed.wakes.wakes[0].wake_reason) != 8 {
        return false
    }
    if closed.endpoints.slots[0].queued_messages != 0 {
        return false
    }
    if closed.endpoints.slots[0].blocked_receiver.waiting != 0 {
        return false
    }
    closed_receive: RuntimeReceiveTransition = attempt_runtime_receive(closed.endpoints, 15)
    if closed_receive.endpoint_closed == 0 {
        return false
    }
    reopened: EndpointTable = install_endpoint(closed.endpoints, 7, 15)
    if reopened.slots[0].active != 1 {
        return false
    }
    if reopened.slots[0].queued_messages != 0 {
        return false
    }
    owner_table: EndpointTable = install_endpoint(empty_table(), 9, 21)
    owner_table = install_endpoint(owner_table, 9, 22)
    owner_blocked: EndpointTable = register_runtime_blocked_sender(owner_table, 21, 1, 41)
    owner_closed: EndpointOwnerCloseTransition = close_runtime_endpoints_for_owner(owner_blocked, 9)
    if owner_closed.closed_count != 2 {
        return false
    }
    if owner_closed.wakes.count != 1 {
        return false
    }
    if wake_reason_score(owner_closed.wakes.wakes[0].wake_reason) != 8 {
        return false
    }
    return true
}

func validate_runtime_frame_copy_boundary() bool {
    table: EndpointTable = install_endpoint(empty_table(), 7, 19)

    first_payload: [4]u8 = zero_payload()
    first_payload[0] = 70
    first_payload[1] = 82
    first_publish: RuntimePublishResult = publish_runtime_frame(table, 19, 7, 21, 2, first_payload)
    if !runtime_publish_succeeded(first_publish.observation) {
        return false
    }
    if first_publish.observation.payload_len != 2 {
        return false
    }
    if first_publish.observation.payload0 != 70 || first_publish.observation.payload1 != 82 {
        return false
    }
    first_receive: RuntimeReceiveResult = receive_runtime_message(first_publish.endpoints, 19)
    if first_receive.available == 0 {
        return false
    }
    if first_receive.message.len != 2 {
        return false
    }
    if first_receive.message.payload[0] != 70 || first_receive.message.payload[1] != 82 {
        return false
    }

    second_payload: [4]u8 = zero_payload()
    second_payload[0] = 65
    second_payload[1] = 77
    second_publish: RuntimePublishResult = publish_runtime_frame(first_receive.endpoints, 19, 7, 22, 2, second_payload)
    if !runtime_publish_succeeded(second_publish.observation) {
        return false
    }
    if second_publish.observation.payload_len != 2 {
        return false
    }
    if second_publish.observation.payload0 != 65 || second_publish.observation.payload1 != 77 {
        return false
    }
    second_receive: RuntimeReceiveResult = receive_runtime_message(second_publish.endpoints, 19)
    if second_receive.available == 0 {
        return false
    }
    if second_receive.message.len != 2 {
        return false
    }
    if second_receive.message.payload[0] != 65 || second_receive.message.payload[1] != 77 {
        return false
    }
    empty_receive: RuntimeReceiveResult = receive_runtime_message(second_receive.endpoints, 19)
    return empty_receive.queue_empty == 1
}
