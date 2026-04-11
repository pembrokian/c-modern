import ipc
import syscall

enum LogMessageTag {
    None,
    Open,
    Ack,
}

struct LogServiceState {
    owner_pid: u32
    endpoint_handle_slot: u32
    last_tag: LogMessageTag
    handled_request_count: usize
    ack_count: usize
    last_client_pid: u32
    last_endpoint_id: u32
    last_request_len: usize
    last_request_byte: u8
    last_ack_byte: u8
}

struct LogHandshakeObservation {
    service_pid: u32
    client_pid: u32
    endpoint_id: u32
    tag: LogMessageTag
    request_len: usize
    request_byte: u8
    ack_byte: u8
    request_count: usize
    ack_count: usize
}

func service_state(owner_pid: u32, endpoint_handle_slot: u32) LogServiceState {
    return LogServiceState{ owner_pid: owner_pid, endpoint_handle_slot: endpoint_handle_slot, last_tag: LogMessageTag.None, handled_request_count: 0, ack_count: 0, last_client_pid: 0, last_endpoint_id: 0, last_request_len: 0, last_request_byte: 0, last_ack_byte: 0 }
}

func record_open_request(state: LogServiceState, observation: syscall.ReceiveObservation) LogServiceState {
    request_byte: u8 = 0
    if observation.payload_len != 0 {
        request_byte = observation.payload[0]
    }
    return LogServiceState{ owner_pid: state.owner_pid, endpoint_handle_slot: state.endpoint_handle_slot, last_tag: LogMessageTag.Open, handled_request_count: state.handled_request_count + 1, ack_count: state.ack_count, last_client_pid: observation.source_pid, last_endpoint_id: observation.endpoint_id, last_request_len: observation.payload_len, last_request_byte: request_byte, last_ack_byte: state.last_ack_byte }
}

func record_ack(state: LogServiceState, ack_byte: u8) LogServiceState {
    return LogServiceState{ owner_pid: state.owner_pid, endpoint_handle_slot: state.endpoint_handle_slot, last_tag: LogMessageTag.Ack, handled_request_count: state.handled_request_count, ack_count: state.ack_count + 1, last_client_pid: state.last_client_pid, last_endpoint_id: state.last_endpoint_id, last_request_len: state.last_request_len, last_request_byte: state.last_request_byte, last_ack_byte: ack_byte }
}

func ack_payload() [4]u8 {
    payload: [4]u8 = ipc.zero_payload()
    payload[0] = 33
    return payload
}

func observe_handshake(state: LogServiceState) LogHandshakeObservation {
    return LogHandshakeObservation{ service_pid: state.owner_pid, client_pid: state.last_client_pid, endpoint_id: state.last_endpoint_id, tag: state.last_tag, request_len: state.last_request_len, request_byte: state.last_request_byte, ack_byte: state.last_ack_byte, request_count: state.handled_request_count, ack_count: state.ack_count }
}

func tag_score(tag: LogMessageTag) i32 {
    switch tag {
    case LogMessageTag.None:
        return 1
    case LogMessageTag.Open:
        return 2
    case LogMessageTag.Ack:
        return 4
    default:
        return 0
    }
    return 0
}