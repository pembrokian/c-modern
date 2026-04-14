import primitives
import syscall

const SERIAL_BUFFER_CAPACITY: usize = 4
const SERIAL_INVALID_BYTE: u8 = 255

struct SerialServiceState {
    owner_pid: u32
    endpoint_handle_slot: u32
    buffer: [4]u8
    len: usize
    reply_status: syscall.SyscallStatus
    reply_len: usize
    reply_payload: [4]u8
}

func serial_init(owner_pid: u32, endpoint_handle_slot: u32) SerialServiceState {
    return SerialServiceState{ owner_pid: owner_pid, endpoint_handle_slot: endpoint_handle_slot, buffer: primitives.zero_payload(), len: 0, reply_status: syscall.SyscallStatus.None, reply_len: 0, reply_payload: primitives.zero_payload() }
}

func service_state(owner_pid: u32, endpoint_handle_slot: u32) SerialServiceState {
    return serial_init(owner_pid, endpoint_handle_slot)
}

func serial_clear_reply(state: SerialServiceState) SerialServiceState {
    return SerialServiceState{ owner_pid: state.owner_pid, endpoint_handle_slot: state.endpoint_handle_slot, buffer: state.buffer, len: state.len, reply_status: syscall.SyscallStatus.None, reply_len: 0, reply_payload: primitives.zero_payload() }
}

func serial_reply_status(state: SerialServiceState) syscall.SyscallStatus {
    return state.reply_status
}

func serial_reply_len(state: SerialServiceState) usize {
    return state.reply_len
}

func serial_reply_payload(state: SerialServiceState) [4]u8 {
    return state.reply_payload
}

func serial_has_pending_reply(state: SerialServiceState) u32 {
    if state.reply_status == syscall.SyscallStatus.None {
        return 0
    }
    return 1
}

func serial_on_receive(state: SerialServiceState, observation: syscall.ReceiveObservation) SerialServiceState {
    next_state: SerialServiceState = serial_clear_reply(state)
    if observation.payload_len == 0 {
        return next_state
    }
    if observation.payload[0] == SERIAL_INVALID_BYTE {
        return next_state
    }
    // Bytes beyond SERIAL_BUFFER_CAPACITY are intentionally dropped: the
    // fixed-size buffer only accommodates one 4-byte command at a time.
    next_buffer: [4]u8 = next_state.buffer
    next_len: usize = next_state.len
    if next_len < SERIAL_BUFFER_CAPACITY {
        next_buffer[next_len] = observation.payload[0]
        next_len = next_len + 1
    }
    if observation.payload_len > 1 && next_len < SERIAL_BUFFER_CAPACITY {
        next_buffer[next_len] = observation.payload[1]
        next_len = next_len + 1
    }
    if observation.payload_len > 2 && next_len < SERIAL_BUFFER_CAPACITY {
        next_buffer[next_len] = observation.payload[2]
        next_len = next_len + 1
    }
    if observation.payload_len > 3 && next_len < SERIAL_BUFFER_CAPACITY {
        next_buffer[next_len] = observation.payload[3]
        next_len = next_len + 1
    }
    return SerialServiceState{ owner_pid: next_state.owner_pid, endpoint_handle_slot: next_state.endpoint_handle_slot, buffer: next_buffer, len: next_len, reply_status: syscall.SyscallStatus.None, reply_len: 0, reply_payload: primitives.zero_payload() }
}

func serial_forward_request_len(state: SerialServiceState) usize {
    return state.len
}

func serial_forward_request_payload(state: SerialServiceState) [4]u8 {
    return state.buffer
}

func serial_clear(state: SerialServiceState) SerialServiceState {
    return SerialServiceState{ owner_pid: state.owner_pid, endpoint_handle_slot: state.endpoint_handle_slot, buffer: primitives.zero_payload(), len: 0, reply_status: state.reply_status, reply_len: state.reply_len, reply_payload: state.reply_payload }
}

func serial_on_reply(state: SerialServiceState, observation: syscall.ReceiveObservation) SerialServiceState {
    if observation.status == syscall.SyscallStatus.None {
        return state
    }
    cleared: SerialServiceState = serial_clear(state)
    return SerialServiceState{ owner_pid: cleared.owner_pid, endpoint_handle_slot: cleared.endpoint_handle_slot, buffer: cleared.buffer, len: cleared.len, reply_status: observation.status, reply_len: observation.payload_len, reply_payload: observation.payload }
}