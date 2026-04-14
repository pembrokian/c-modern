import ipc
import syscall

const SERIAL_CONSUME_LOG_CAPACITY: usize = 4
const SERIAL_INVALID_BYTE: u8 = 255

enum SerialMessageTag {
    None,
    Ingress,
    Malformed,
}

struct SerialServiceState {
    owner_pid: u32
    endpoint_handle_slot: u32
    last_tag: SerialMessageTag
    handled_ingress_count: usize
    malformed_ingress_count: usize
    last_source_pid: u32
    last_endpoint_id: u32
    last_payload_len: usize
    last_received_byte: u8
    consumed_log_len: usize
    total_consumed_bytes: usize
    consumed_log: [4]u8
    forward_endpoint_id: u32
    forward_status: syscall.SyscallStatus
    forwarded_request_len: usize
    forwarded_request_byte0: u8
    forwarded_request_byte1: u8
    forwarded_request_count: usize
    aggregate_reply_status: syscall.SyscallStatus
    aggregate_reply_len: usize
    aggregate_reply_byte0: u8
    aggregate_reply_byte1: u8
    aggregate_reply_byte2: u8
    aggregate_reply_byte3: u8
    aggregate_reply_count: usize
}

struct SerialIngressObservation {
    service_pid: u32
    source_pid: u32
    endpoint_id: u32
    tag: SerialMessageTag
    payload_len: usize
    received_byte: u8
    ingress_count: usize
    malformed_ingress_count: usize
    log_len: usize
    total_consumed_bytes: usize
    log_byte0: u8
    log_byte1: u8
    log_byte2: u8
    log_byte3: u8
}

struct SerialCompositionObservation {
    service_pid: u32
    forward_endpoint_id: u32
    forward_status: syscall.SyscallStatus
    forwarded_request_len: usize
    forwarded_request_byte0: u8
    forwarded_request_byte1: u8
    forwarded_request_count: usize
    aggregate_reply_status: syscall.SyscallStatus
    aggregate_reply_len: usize
    aggregate_reply_byte0: u8
    aggregate_reply_byte1: u8
    aggregate_reply_byte2: u8
    aggregate_reply_byte3: u8
    aggregate_reply_count: usize
}

func service_state(owner_pid: u32, endpoint_handle_slot: u32) SerialServiceState {
    return SerialServiceState{ owner_pid: owner_pid, endpoint_handle_slot: endpoint_handle_slot, last_tag: SerialMessageTag.None, handled_ingress_count: 0, malformed_ingress_count: 0, last_source_pid: 0, last_endpoint_id: 0, last_payload_len: 0, last_received_byte: 0, consumed_log_len: 0, total_consumed_bytes: 0, consumed_log: ipc.zero_payload(), forward_endpoint_id: 0, forward_status: syscall.empty_send_observation().status, forwarded_request_len: 0, forwarded_request_byte0: 0, forwarded_request_byte1: 0, forwarded_request_count: 0, aggregate_reply_status: syscall.empty_receive_observation().status, aggregate_reply_len: 0, aggregate_reply_byte0: 0, aggregate_reply_byte1: 0, aggregate_reply_byte2: 0, aggregate_reply_byte3: 0, aggregate_reply_count: 0 }
}

func append_payload_to_log(log: [4]u8, log_len: usize, payload_len: usize, payload: [4]u8) [4]u8 {
    next_log: [4]u8 = log
    if payload_len != 0 && log_len < SERIAL_CONSUME_LOG_CAPACITY {
        next_log[log_len] = payload[0]
    }
    if payload_len > 1 && log_len + 1 < SERIAL_CONSUME_LOG_CAPACITY {
        next_log[log_len + 1] = payload[1]
    }
    if payload_len > 2 && log_len + 2 < SERIAL_CONSUME_LOG_CAPACITY {
        next_log[log_len + 2] = payload[2]
    }
    if payload_len > 3 && log_len + 3 < SERIAL_CONSUME_LOG_CAPACITY {
        next_log[log_len + 3] = payload[3]
    }
    return next_log
}

func appended_log_len(log_len: usize, payload_len: usize) usize {
    next_len: usize = log_len
    if payload_len != 0 && next_len < SERIAL_CONSUME_LOG_CAPACITY {
        next_len = next_len + 1
    }
    if payload_len > 1 && next_len < SERIAL_CONSUME_LOG_CAPACITY {
        next_len = next_len + 1
    }
    if payload_len > 2 && next_len < SERIAL_CONSUME_LOG_CAPACITY {
        next_len = next_len + 1
    }
    if payload_len > 3 && next_len < SERIAL_CONSUME_LOG_CAPACITY {
        next_len = next_len + 1
    }
    return next_len
}

func malformed_ingress(payload_len: usize, payload: [4]u8) bool {
    if payload_len == 0 {
        return true
    }
    return payload[0] == SERIAL_INVALID_BYTE
}

func record_ingress(state: SerialServiceState, observation: syscall.ReceiveObservation) SerialServiceState {
    received_byte: u8 = 0
    if observation.payload_len != 0 {
        received_byte = observation.payload[0]
    }
    if malformed_ingress(observation.payload_len, observation.payload) {
        return SerialServiceState{ owner_pid: state.owner_pid, endpoint_handle_slot: state.endpoint_handle_slot, last_tag: SerialMessageTag.Malformed, handled_ingress_count: state.handled_ingress_count + 1, malformed_ingress_count: state.malformed_ingress_count + 1, last_source_pid: observation.source_pid, last_endpoint_id: observation.endpoint_id, last_payload_len: observation.payload_len, last_received_byte: received_byte, consumed_log_len: state.consumed_log_len, total_consumed_bytes: state.total_consumed_bytes, consumed_log: state.consumed_log, forward_endpoint_id: state.forward_endpoint_id, forward_status: state.forward_status, forwarded_request_len: state.forwarded_request_len, forwarded_request_byte0: state.forwarded_request_byte0, forwarded_request_byte1: state.forwarded_request_byte1, forwarded_request_count: state.forwarded_request_count, aggregate_reply_status: state.aggregate_reply_status, aggregate_reply_len: state.aggregate_reply_len, aggregate_reply_byte0: state.aggregate_reply_byte0, aggregate_reply_byte1: state.aggregate_reply_byte1, aggregate_reply_byte2: state.aggregate_reply_byte2, aggregate_reply_byte3: state.aggregate_reply_byte3, aggregate_reply_count: state.aggregate_reply_count }
    }
    next_log: [4]u8 = append_payload_to_log(state.consumed_log, state.consumed_log_len, observation.payload_len, observation.payload)
    next_log_len: usize = appended_log_len(state.consumed_log_len, observation.payload_len)
    return SerialServiceState{ owner_pid: state.owner_pid, endpoint_handle_slot: state.endpoint_handle_slot, last_tag: SerialMessageTag.Ingress, handled_ingress_count: state.handled_ingress_count + 1, malformed_ingress_count: state.malformed_ingress_count, last_source_pid: observation.source_pid, last_endpoint_id: observation.endpoint_id, last_payload_len: observation.payload_len, last_received_byte: received_byte, consumed_log_len: next_log_len, total_consumed_bytes: state.total_consumed_bytes + observation.payload_len, consumed_log: next_log, forward_endpoint_id: state.forward_endpoint_id, forward_status: state.forward_status, forwarded_request_len: state.forwarded_request_len, forwarded_request_byte0: state.forwarded_request_byte0, forwarded_request_byte1: state.forwarded_request_byte1, forwarded_request_count: state.forwarded_request_count, aggregate_reply_status: state.aggregate_reply_status, aggregate_reply_len: state.aggregate_reply_len, aggregate_reply_byte0: state.aggregate_reply_byte0, aggregate_reply_byte1: state.aggregate_reply_byte1, aggregate_reply_byte2: state.aggregate_reply_byte2, aggregate_reply_byte3: state.aggregate_reply_byte3, aggregate_reply_count: state.aggregate_reply_count }
}

func empty_ingress_observation() SerialIngressObservation {
    return SerialIngressObservation{ service_pid: 0, source_pid: 0, endpoint_id: 0, tag: SerialMessageTag.None, payload_len: 0, received_byte: 0, ingress_count: 0, malformed_ingress_count: 0, log_len: 0, total_consumed_bytes: 0, log_byte0: 0, log_byte1: 0, log_byte2: 0, log_byte3: 0 }
}

func empty_serial_message_tag() SerialMessageTag {
    return SerialMessageTag.None
}

func observe_ingress(state: SerialServiceState) SerialIngressObservation {
    return SerialIngressObservation{ service_pid: state.owner_pid, source_pid: state.last_source_pid, endpoint_id: state.last_endpoint_id, tag: state.last_tag, payload_len: state.last_payload_len, received_byte: state.last_received_byte, ingress_count: state.handled_ingress_count, malformed_ingress_count: state.malformed_ingress_count, log_len: state.consumed_log_len, total_consumed_bytes: state.total_consumed_bytes, log_byte0: state.consumed_log[0], log_byte1: state.consumed_log[1], log_byte2: state.consumed_log[2], log_byte3: state.consumed_log[3] }
}

func empty_composition_observation() SerialCompositionObservation {
    return SerialCompositionObservation{ service_pid: 0, forward_endpoint_id: 0, forward_status: syscall.empty_send_observation().status, forwarded_request_len: 0, forwarded_request_byte0: 0, forwarded_request_byte1: 0, forwarded_request_count: 0, aggregate_reply_status: syscall.empty_receive_observation().status, aggregate_reply_len: 0, aggregate_reply_byte0: 0, aggregate_reply_byte1: 0, aggregate_reply_byte2: 0, aggregate_reply_byte3: 0, aggregate_reply_count: 0 }
}

func forwarded_request_len(payload_len: usize) usize {
    if payload_len == 0 {
        return 0
    }
    if payload_len == 1 {
        return 1
    }
    return 2
}

func forwarded_request_payload(payload_len: usize, payload: [4]u8) [4]u8 {
    request: [4]u8 = ipc.zero_payload()
    if payload_len != 0 {
        request[0] = payload[0]
    }
    if payload_len > 1 {
        request[1] = payload[1]
    }
    return request
}

func record_forward(state: SerialServiceState, endpoint_id: u32, status: syscall.SyscallStatus, payload_len: usize, payload: [4]u8) SerialServiceState {
    return SerialServiceState{ owner_pid: state.owner_pid, endpoint_handle_slot: state.endpoint_handle_slot, last_tag: state.last_tag, handled_ingress_count: state.handled_ingress_count, malformed_ingress_count: state.malformed_ingress_count, last_source_pid: state.last_source_pid, last_endpoint_id: state.last_endpoint_id, last_payload_len: state.last_payload_len, last_received_byte: state.last_received_byte, consumed_log_len: state.consumed_log_len, total_consumed_bytes: state.total_consumed_bytes, consumed_log: state.consumed_log, forward_endpoint_id: endpoint_id, forward_status: status, forwarded_request_len: payload_len, forwarded_request_byte0: payload[0], forwarded_request_byte1: payload[1], forwarded_request_count: state.forwarded_request_count + 1, aggregate_reply_status: state.aggregate_reply_status, aggregate_reply_len: state.aggregate_reply_len, aggregate_reply_byte0: state.aggregate_reply_byte0, aggregate_reply_byte1: state.aggregate_reply_byte1, aggregate_reply_byte2: state.aggregate_reply_byte2, aggregate_reply_byte3: state.aggregate_reply_byte3, aggregate_reply_count: state.aggregate_reply_count }
}

func record_aggregate_reply(state: SerialServiceState, observation: syscall.ReceiveObservation) SerialServiceState {
    return SerialServiceState{ owner_pid: state.owner_pid, endpoint_handle_slot: state.endpoint_handle_slot, last_tag: state.last_tag, handled_ingress_count: state.handled_ingress_count, malformed_ingress_count: state.malformed_ingress_count, last_source_pid: state.last_source_pid, last_endpoint_id: state.last_endpoint_id, last_payload_len: state.last_payload_len, last_received_byte: state.last_received_byte, consumed_log_len: state.consumed_log_len, total_consumed_bytes: state.total_consumed_bytes, consumed_log: state.consumed_log, forward_endpoint_id: state.forward_endpoint_id, forward_status: state.forward_status, forwarded_request_len: state.forwarded_request_len, forwarded_request_byte0: state.forwarded_request_byte0, forwarded_request_byte1: state.forwarded_request_byte1, forwarded_request_count: state.forwarded_request_count, aggregate_reply_status: observation.status, aggregate_reply_len: observation.payload_len, aggregate_reply_byte0: observation.payload[0], aggregate_reply_byte1: observation.payload[1], aggregate_reply_byte2: observation.payload[2], aggregate_reply_byte3: observation.payload[3], aggregate_reply_count: state.aggregate_reply_count + 1 }
}

func observe_composition(state: SerialServiceState) SerialCompositionObservation {
    return SerialCompositionObservation{ service_pid: state.owner_pid, forward_endpoint_id: state.forward_endpoint_id, forward_status: state.forward_status, forwarded_request_len: state.forwarded_request_len, forwarded_request_byte0: state.forwarded_request_byte0, forwarded_request_byte1: state.forwarded_request_byte1, forwarded_request_count: state.forwarded_request_count, aggregate_reply_status: state.aggregate_reply_status, aggregate_reply_len: state.aggregate_reply_len, aggregate_reply_byte0: state.aggregate_reply_byte0, aggregate_reply_byte1: state.aggregate_reply_byte1, aggregate_reply_byte2: state.aggregate_reply_byte2, aggregate_reply_byte3: state.aggregate_reply_byte3, aggregate_reply_count: state.aggregate_reply_count }
}

func tag_score(tag: SerialMessageTag) i32 {
    switch tag {
    case SerialMessageTag.None:
        return 1
    case SerialMessageTag.Ingress:
        return 2
    case SerialMessageTag.Malformed:
        return 4
    default:
        return 0
    }
    return 0
}
