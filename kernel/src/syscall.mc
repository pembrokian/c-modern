import capability
import endpoint

enum SyscallId {
    None,
    Send,
    Receive,
    Spawn,
}

enum SyscallStatus {
    None,
    Ok,
    WouldBlock,
    InvalidHandle,
    InvalidEndpoint,
    Closed,
}

struct SyscallGate {
    open: u32
    last_id: SyscallId
    last_status: SyscallStatus
    send_count: u32
    receive_count: u32
}

struct SendRequest {
    handle_slot: u32
    payload_len: usize
    payload: [4]u8
}

struct ReceiveRequest {
    handle_slot: u32
}

struct ReceiveObservation {
    status: SyscallStatus
    endpoint_id: u32
    source_pid: u32
    payload_len: usize
    payload: [4]u8
}

struct SendResult {
    gate: SyscallGate
    endpoints: endpoint.EndpointTable
    status: SyscallStatus
}

struct ReceiveResult {
    gate: SyscallGate
    endpoints: endpoint.EndpointTable
    observation: ReceiveObservation
}

func gate_closed() SyscallGate {
    return SyscallGate{ open: 0, last_id: SyscallId.None, last_status: SyscallStatus.None, send_count: 0, receive_count: 0 }
}

func open_gate(gate: SyscallGate) SyscallGate {
    return SyscallGate{ open: 1, last_id: gate.last_id, last_status: gate.last_status, send_count: gate.send_count, receive_count: gate.receive_count }
}

func build_send_request(handle_slot: u32, payload_len: usize, payload: [4]u8) SendRequest {
    return SendRequest{ handle_slot: handle_slot, payload_len: payload_len, payload: payload }
}

func build_receive_request(handle_slot: u32) ReceiveRequest {
    return ReceiveRequest{ handle_slot: handle_slot }
}

func empty_receive_observation() ReceiveObservation {
    return ReceiveObservation{ status: SyscallStatus.None, endpoint_id: 0, source_pid: 0, payload_len: 0, payload: endpoint.zero_payload() }
}

func send_result(gate: SyscallGate, endpoints: endpoint.EndpointTable, status: SyscallStatus) SendResult {
    return SendResult{ gate: gate, endpoints: endpoints, status: status }
}

func receive_result(gate: SyscallGate, endpoints: endpoint.EndpointTable, observation: ReceiveObservation) ReceiveResult {
    return ReceiveResult{ gate: gate, endpoints: endpoints, observation: observation }
}

func update_gate(gate: SyscallGate, id: SyscallId, status: SyscallStatus, send_delta: u32, receive_delta: u32) SyscallGate {
    return SyscallGate{ open: gate.open, last_id: id, last_status: status, send_count: gate.send_count + send_delta, receive_count: gate.receive_count + receive_delta }
}

func perform_send(gate: SyscallGate, handle_table: capability.HandleTable, endpoints: endpoint.EndpointTable, sender_pid: u32, request: SendRequest) SendResult {
    if gate.open == 0 {
        return send_result(update_gate(gate, SyscallId.Send, SyscallStatus.Closed, 0, 0), endpoints, SyscallStatus.Closed)
    }
    endpoint_id: u32 = capability.find_endpoint_for_handle(handle_table, request.handle_slot)
    if endpoint_id == 0 {
        return send_result(update_gate(gate, SyscallId.Send, SyscallStatus.InvalidHandle, 0, 0), endpoints, SyscallStatus.InvalidHandle)
    }
    endpoint_index: usize = endpoint.find_endpoint_index(endpoints, endpoint_id)
    if endpoint_index >= endpoints.count {
        return send_result(update_gate(gate, SyscallId.Send, SyscallStatus.InvalidEndpoint, 0, 0), endpoints, SyscallStatus.InvalidEndpoint)
    }
    message_id: u32 = gate.send_count + 2
    updated_endpoints: endpoint.EndpointTable = endpoint.enqueue_message(endpoints, endpoint_index, endpoint.byte_message(message_id, sender_pid, endpoint_id, request.payload_len, request.payload))
    return send_result(update_gate(gate, SyscallId.Send, SyscallStatus.Ok, 1, 0), updated_endpoints, SyscallStatus.Ok)
}

func perform_receive(gate: SyscallGate, handle_table: capability.HandleTable, endpoints: endpoint.EndpointTable, request: ReceiveRequest) ReceiveResult {
    if gate.open == 0 {
        return receive_result(update_gate(gate, SyscallId.Receive, SyscallStatus.Closed, 0, 0), endpoints, ReceiveObservation{ status: SyscallStatus.Closed, endpoint_id: 0, source_pid: 0, payload_len: 0, payload: endpoint.zero_payload() })
    }
    endpoint_id: u32 = capability.find_endpoint_for_handle(handle_table, request.handle_slot)
    if endpoint_id == 0 {
        return receive_result(update_gate(gate, SyscallId.Receive, SyscallStatus.InvalidHandle, 0, 0), endpoints, ReceiveObservation{ status: SyscallStatus.InvalidHandle, endpoint_id: 0, source_pid: 0, payload_len: 0, payload: endpoint.zero_payload() })
    }
    endpoint_index: usize = endpoint.find_endpoint_index(endpoints, endpoint_id)
    if endpoint_index >= endpoints.count {
        return receive_result(update_gate(gate, SyscallId.Receive, SyscallStatus.InvalidEndpoint, 0, 0), endpoints, ReceiveObservation{ status: SyscallStatus.InvalidEndpoint, endpoint_id: endpoint_id, source_pid: 0, payload_len: 0, payload: endpoint.zero_payload() })
    }
    message: endpoint.KernelMessage = endpoint.peek_head_message(endpoints, endpoint_index)
    if endpoint.message_state_score(message.state) == 1 {
        return receive_result(update_gate(gate, SyscallId.Receive, SyscallStatus.WouldBlock, 0, 0), endpoints, ReceiveObservation{ status: SyscallStatus.WouldBlock, endpoint_id: endpoint_id, source_pid: 0, payload_len: 0, payload: endpoint.zero_payload() })
    }
    observation: ReceiveObservation = ReceiveObservation{ status: SyscallStatus.Ok, endpoint_id: message.endpoint_id, source_pid: message.source_pid, payload_len: message.len, payload: message.payload }
    updated_endpoints: endpoint.EndpointTable = endpoint.consume_head_message(endpoints, endpoint_index)
    return receive_result(update_gate(gate, SyscallId.Receive, SyscallStatus.Ok, 0, 1), updated_endpoints, observation)
}

func id_score(id: SyscallId) i32 {
    switch id {
    case SyscallId.None:
        return 1
    case SyscallId.Send:
        return 2
    case SyscallId.Receive:
        return 4
    case SyscallId.Spawn:
        return 8
    default:
        return 0
    }
    return 0
}

func status_score(status: SyscallStatus) i32 {
    switch status {
    case SyscallStatus.None:
        return 1
    case SyscallStatus.Ok:
        return 2
    case SyscallStatus.WouldBlock:
        return 4
    case SyscallStatus.InvalidHandle:
        return 8
    case SyscallStatus.InvalidEndpoint:
        return 16
    case SyscallStatus.Closed:
        return 32
    default:
        return 0
    }
    return 0
}