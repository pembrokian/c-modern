import ipc
import syscall

const RETAINED_LOG_CAPACITY: usize = 4

enum LogMessageTag {
    None,
    Open,
    Append,
    Tail,
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
    append_count: usize
    tail_query_count: usize
    overwrite_count: usize
    retained_len: usize
    retained_log: [4]u8
    tail_byte0: u8
    tail_byte1: u8
    overflowed_last_request: u32
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

struct LogRetentionObservation {
    service_pid: u32
    client_pid: u32
    endpoint_id: u32
    tag: LogMessageTag
    retained_len: usize
    append_count: usize
    tail_query_count: usize
    overwrite_count: usize
    overflowed_last_request: u32
    retained_byte0: u8
    retained_byte1: u8
    retained_byte2: u8
    retained_byte3: u8
    tail_byte0: u8
    tail_byte1: u8
}

func service_state(owner_pid: u32, endpoint_handle_slot: u32) LogServiceState {
    return LogServiceState{ owner_pid: owner_pid, endpoint_handle_slot: endpoint_handle_slot, last_tag: LogMessageTag.None, handled_request_count: 0, ack_count: 0, last_client_pid: 0, last_endpoint_id: 0, last_request_len: 0, last_request_byte: 0, last_ack_byte: 0, append_count: 0, tail_query_count: 0, overwrite_count: 0, retained_len: 0, retained_log: ipc.zero_payload(), tail_byte0: 0, tail_byte1: 33, overflowed_last_request: 0 }
}

func append_retained_byte(log: [4]u8, log_len: usize, value: u8) [4]u8 {
    next_log: [4]u8 = log
    if log_len < RETAINED_LOG_CAPACITY {
        next_log[log_len] = value
        return next_log
    }
    next_log[0] = log[1]
    next_log[1] = log[2]
    next_log[2] = log[3]
    next_log[3] = value
    return next_log
}

func appended_retained_len(log_len: usize) usize {
    if log_len < RETAINED_LOG_CAPACITY {
        return log_len + 1
    }
    return RETAINED_LOG_CAPACITY
}

func tail_byte0_for(log: [4]u8, log_len: usize) u8 {
    if log_len == 0 {
        return 0
    }
    return log[log_len - 1]
}

func tail_byte1_for(log: [4]u8, log_len: usize, ack_byte: u8) u8 {
    if log_len > 1 {
        return log[log_len - 2]
    }
    return ack_byte
}

func record_append_request(state: LogServiceState, observation: syscall.ReceiveObservation) LogServiceState {
    request_byte: u8 = 0
    if observation.payload_len != 0 {
        request_byte = observation.payload[0]
    }
    next_log: [4]u8 = append_retained_byte(state.retained_log, state.retained_len, request_byte)
    next_len: usize = appended_retained_len(state.retained_len)
    overflowed_last_request: u32 = 0
    overwrite_count: usize = state.overwrite_count
    if state.retained_len == RETAINED_LOG_CAPACITY {
        overflowed_last_request = 1
        overwrite_count = overwrite_count + 1
    }
    tail_byte0: u8 = tail_byte0_for(next_log, next_len)
    tail_byte1: u8 = tail_byte1_for(next_log, next_len, state.last_ack_byte)
    return LogServiceState{ owner_pid: state.owner_pid, endpoint_handle_slot: state.endpoint_handle_slot, last_tag: LogMessageTag.Append, handled_request_count: state.handled_request_count + 1, ack_count: state.ack_count, last_client_pid: observation.source_pid, last_endpoint_id: observation.endpoint_id, last_request_len: observation.payload_len, last_request_byte: request_byte, last_ack_byte: state.last_ack_byte, append_count: state.append_count + 1, tail_query_count: state.tail_query_count, overwrite_count: overwrite_count, retained_len: next_len, retained_log: next_log, tail_byte0: tail_byte0, tail_byte1: tail_byte1, overflowed_last_request: overflowed_last_request }
}

func record_open_request(state: LogServiceState, observation: syscall.ReceiveObservation) LogServiceState {
    request_byte: u8 = 0
    if observation.payload_len != 0 {
        request_byte = observation.payload[0]
    }
    return LogServiceState{ owner_pid: state.owner_pid, endpoint_handle_slot: state.endpoint_handle_slot, last_tag: LogMessageTag.Open, handled_request_count: state.handled_request_count + 1, ack_count: state.ack_count, last_client_pid: observation.source_pid, last_endpoint_id: observation.endpoint_id, last_request_len: observation.payload_len, last_request_byte: request_byte, last_ack_byte: state.last_ack_byte, append_count: state.append_count, tail_query_count: state.tail_query_count, overwrite_count: state.overwrite_count, retained_len: state.retained_len, retained_log: state.retained_log, tail_byte0: state.tail_byte0, tail_byte1: state.tail_byte1, overflowed_last_request: 0 }
}

func record_tail_query(state: LogServiceState, client_pid: u32, endpoint_id: u32) LogServiceState {
    return LogServiceState{ owner_pid: state.owner_pid, endpoint_handle_slot: state.endpoint_handle_slot, last_tag: LogMessageTag.Tail, handled_request_count: state.handled_request_count + 1, ack_count: state.ack_count, last_client_pid: client_pid, last_endpoint_id: endpoint_id, last_request_len: 0, last_request_byte: state.last_request_byte, last_ack_byte: state.last_ack_byte, append_count: state.append_count, tail_query_count: state.tail_query_count + 1, overwrite_count: state.overwrite_count, retained_len: state.retained_len, retained_log: state.retained_log, tail_byte0: tail_byte0_for(state.retained_log, state.retained_len), tail_byte1: tail_byte1_for(state.retained_log, state.retained_len, state.last_ack_byte), overflowed_last_request: 0 }
}

func record_ack(state: LogServiceState, ack_byte: u8) LogServiceState {
    return LogServiceState{ owner_pid: state.owner_pid, endpoint_handle_slot: state.endpoint_handle_slot, last_tag: LogMessageTag.Ack, handled_request_count: state.handled_request_count, ack_count: state.ack_count + 1, last_client_pid: state.last_client_pid, last_endpoint_id: state.last_endpoint_id, last_request_len: state.last_request_len, last_request_byte: state.last_request_byte, last_ack_byte: ack_byte, append_count: state.append_count, tail_query_count: state.tail_query_count, overwrite_count: state.overwrite_count, retained_len: state.retained_len, retained_log: state.retained_log, tail_byte0: state.tail_byte0, tail_byte1: state.tail_byte1, overflowed_last_request: state.overflowed_last_request }
}

func ack_payload() [4]u8 {
    payload: [4]u8 = ipc.zero_payload()
    payload[0] = 33
    return payload
}

func reply_payload(state: LogServiceState) [4]u8 {
    payload: [4]u8 = ipc.zero_payload()
    switch state.last_tag {
    case LogMessageTag.Tail:
        payload[0] = state.tail_byte0
        payload[1] = state.tail_byte1
        return payload
    default:
        payload = ack_payload()
        if state.overflowed_last_request != 0 {
            payload[1] = 1
        }
        return payload
    }
    return payload
}

func observe_handshake(state: LogServiceState) LogHandshakeObservation {
    return LogHandshakeObservation{ service_pid: state.owner_pid, client_pid: state.last_client_pid, endpoint_id: state.last_endpoint_id, tag: state.last_tag, request_len: state.last_request_len, request_byte: state.last_request_byte, ack_byte: state.last_ack_byte, request_count: state.handled_request_count, ack_count: state.ack_count }
}

func observe_retention(state: LogServiceState) LogRetentionObservation {
    return LogRetentionObservation{ service_pid: state.owner_pid, client_pid: state.last_client_pid, endpoint_id: state.last_endpoint_id, tag: state.last_tag, retained_len: state.retained_len, append_count: state.append_count, tail_query_count: state.tail_query_count, overwrite_count: state.overwrite_count, overflowed_last_request: state.overflowed_last_request, retained_byte0: state.retained_log[0], retained_byte1: state.retained_log[1], retained_byte2: state.retained_log[2], retained_byte3: state.retained_log[3], tail_byte0: state.tail_byte0, tail_byte1: state.tail_byte1 }
}

func tag_score(tag: LogMessageTag) i32 {
    switch tag {
    case LogMessageTag.None:
        return 1
    case LogMessageTag.Open:
        return 2
    case LogMessageTag.Append:
        return 4
    case LogMessageTag.Tail:
        return 8
    case LogMessageTag.Ack:
        return 16
    default:
        return 0
    }
    return 0
}