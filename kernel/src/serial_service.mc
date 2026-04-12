import ipc
import syscall

const SERIAL_CONSUME_LOG_CAPACITY: usize = 4

enum SerialMessageTag {
    None,
    Ingress,
}

struct SerialServiceState {
    owner_pid: u32
    endpoint_handle_slot: u32
    last_tag: SerialMessageTag
    handled_ingress_count: usize
    last_source_pid: u32
    last_endpoint_id: u32
    last_payload_len: usize
    last_received_byte: u8
    consumed_log_len: usize
    total_consumed_bytes: usize
    consumed_log: [4]u8
}

struct SerialIngressObservation {
    service_pid: u32
    source_pid: u32
    endpoint_id: u32
    payload_len: usize
    received_byte: u8
    ingress_count: usize
    log_len: usize
    total_consumed_bytes: usize
    log_byte0: u8
    log_byte1: u8
    log_byte2: u8
    log_byte3: u8
}

func service_state(owner_pid: u32, endpoint_handle_slot: u32) SerialServiceState {
    return SerialServiceState{ owner_pid: owner_pid, endpoint_handle_slot: endpoint_handle_slot, last_tag: SerialMessageTag.None, handled_ingress_count: 0, last_source_pid: 0, last_endpoint_id: 0, last_payload_len: 0, last_received_byte: 0, consumed_log_len: 0, total_consumed_bytes: 0, consumed_log: ipc.zero_payload() }
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

func record_ingress(state: SerialServiceState, observation: syscall.ReceiveObservation) SerialServiceState {
    received_byte: u8 = 0
    if observation.payload_len != 0 {
        received_byte = observation.payload[0]
    }
    next_log: [4]u8 = append_payload_to_log(state.consumed_log, state.consumed_log_len, observation.payload_len, observation.payload)
    next_log_len: usize = appended_log_len(state.consumed_log_len, observation.payload_len)
    return SerialServiceState{ owner_pid: state.owner_pid, endpoint_handle_slot: state.endpoint_handle_slot, last_tag: SerialMessageTag.Ingress, handled_ingress_count: state.handled_ingress_count + 1, last_source_pid: observation.source_pid, last_endpoint_id: observation.endpoint_id, last_payload_len: observation.payload_len, last_received_byte: received_byte, consumed_log_len: next_log_len, total_consumed_bytes: state.total_consumed_bytes + observation.payload_len, consumed_log: next_log }
}

func empty_ingress_observation() SerialIngressObservation {
    return SerialIngressObservation{ service_pid: 0, source_pid: 0, endpoint_id: 0, payload_len: 0, received_byte: 0, ingress_count: 0, log_len: 0, total_consumed_bytes: 0, log_byte0: 0, log_byte1: 0, log_byte2: 0, log_byte3: 0 }
}

func observe_ingress(state: SerialServiceState) SerialIngressObservation {
    return SerialIngressObservation{ service_pid: state.owner_pid, source_pid: state.last_source_pid, endpoint_id: state.last_endpoint_id, payload_len: state.last_payload_len, received_byte: state.last_received_byte, ingress_count: state.handled_ingress_count, log_len: state.consumed_log_len, total_consumed_bytes: state.total_consumed_bytes, log_byte0: state.consumed_log[0], log_byte1: state.consumed_log[1], log_byte2: state.consumed_log[2], log_byte3: state.consumed_log[3] }
}

func tag_score(tag: SerialMessageTag) i32 {
    switch tag {
    case SerialMessageTag.None:
        return 1
    case SerialMessageTag.Ingress:
        return 2
    default:
        return 0
    }
    return 0
}
