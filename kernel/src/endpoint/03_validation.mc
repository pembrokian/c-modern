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
    return true
}