import ipc
import syscall

const SERIAL_BUFFER_CAPACITY: usize = 4
const SERIAL_INVALID_BYTE: u8 = 255

struct SerialServiceState {
    owner_pid: u32
    endpoint_handle_slot: u32
    buffer: [4]u8
    len: usize
}

func serial_init(owner_pid: u32, endpoint_handle_slot: u32) SerialServiceState {
    return SerialServiceState{ owner_pid: owner_pid, endpoint_handle_slot: endpoint_handle_slot, buffer: ipc.zero_payload(), len: 0 }
}

func service_state(owner_pid: u32, endpoint_handle_slot: u32) SerialServiceState {
    return serial_init(owner_pid, endpoint_handle_slot)
}

func serial_on_receive(state: SerialServiceState, observation: syscall.ReceiveObservation) SerialServiceState {
    if observation.payload_len == 0 {
        return state
    }
    if observation.payload[0] == SERIAL_INVALID_BYTE {
        return state
    }
    next_buffer: [4]u8 = state.buffer
    next_len: usize = state.len
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
    return SerialServiceState{ owner_pid: state.owner_pid, endpoint_handle_slot: state.endpoint_handle_slot, buffer: next_buffer, len: next_len }
}

func serial_forward_request_len(state: SerialServiceState) usize {
    return state.len
}

func serial_forward_request_payload(state: SerialServiceState) [4]u8 {
    return state.buffer
}

func serial_clear(state: SerialServiceState) SerialServiceState {
    return SerialServiceState{ owner_pid: state.owner_pid, endpoint_handle_slot: state.endpoint_handle_slot, buffer: ipc.zero_payload(), len: 0 }
}

func serial_on_reply(state: SerialServiceState, observation: syscall.ReceiveObservation) SerialServiceState {
    if observation.payload_len == 0 {
        return state
    }
    return serial_clear(state)
}

func debug_ingress(observation: syscall.ReceiveObservation) u32 {
    if observation.payload_len == 0 {
        return 0
    }
    return 1
}