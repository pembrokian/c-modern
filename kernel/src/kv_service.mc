import ipc
import syscall

const KV_CAPACITY: usize = 4

struct KvServiceState {
    owner_pid: u32
    endpoint_handle_slot: u32
    keys: [4]u8
    values: [4]u8
    count: usize
}

struct KvReply {
    status: syscall.SyscallStatus
    payload_len: usize
    payload: [4]u8
}

struct KvReceiveResult {
    state: KvServiceState
    reply: KvReply
}

func kv_init(owner_pid: u32, endpoint_handle_slot: u32) KvServiceState {
    return KvServiceState{ owner_pid: owner_pid, endpoint_handle_slot: endpoint_handle_slot, keys: ipc.zero_payload(), values: ipc.zero_payload(), count: 0 }
}

func service_state(owner_pid: u32, endpoint_handle_slot: u32) KvServiceState {
    return kv_init(owner_pid, endpoint_handle_slot)
}

func kv_on_set(state: KvServiceState, observation: syscall.ReceiveObservation) KvServiceState {
    if observation.payload_len < 2 {
        return state
    }
    next_keys: [4]u8 = state.keys
    next_values: [4]u8 = state.values
    if state.count > 0 && next_keys[0] == observation.payload[0] {
        next_values[0] = observation.payload[1]
        return KvServiceState{ owner_pid: state.owner_pid, endpoint_handle_slot: state.endpoint_handle_slot, keys: next_keys, values: next_values, count: state.count }
    }
    if state.count > 1 && next_keys[1] == observation.payload[0] {
        next_values[1] = observation.payload[1]
        return KvServiceState{ owner_pid: state.owner_pid, endpoint_handle_slot: state.endpoint_handle_slot, keys: next_keys, values: next_values, count: state.count }
    }
    if state.count > 2 && next_keys[2] == observation.payload[0] {
        next_values[2] = observation.payload[1]
        return KvServiceState{ owner_pid: state.owner_pid, endpoint_handle_slot: state.endpoint_handle_slot, keys: next_keys, values: next_values, count: state.count }
    }
    if state.count > 3 && next_keys[3] == observation.payload[0] {
        next_values[3] = observation.payload[1]
        return KvServiceState{ owner_pid: state.owner_pid, endpoint_handle_slot: state.endpoint_handle_slot, keys: next_keys, values: next_values, count: state.count }
    }
    if state.count >= KV_CAPACITY {
        return state
    }
    next_keys[state.count] = observation.payload[0]
    next_values[state.count] = observation.payload[1]
    return KvServiceState{ owner_pid: state.owner_pid, endpoint_handle_slot: state.endpoint_handle_slot, keys: next_keys, values: next_values, count: state.count + 1 }
}

func kv_on_get(state: KvServiceState, observation: syscall.ReceiveObservation) KvReply {
    reply_payload: [4]u8 = ipc.zero_payload()
    if observation.payload_len == 0 {
        return KvReply{ status: syscall.SyscallStatus.InvalidArgument, payload_len: 0, payload: reply_payload }
    }
    if state.count > 0 && state.keys[0] == observation.payload[0] {
        reply_payload[0] = observation.payload[0]
        reply_payload[1] = state.values[0]
        return KvReply{ status: syscall.SyscallStatus.Ok, payload_len: 2, payload: reply_payload }
    }
    if state.count > 1 && state.keys[1] == observation.payload[0] {
        reply_payload[0] = observation.payload[0]
        reply_payload[1] = state.values[1]
        return KvReply{ status: syscall.SyscallStatus.Ok, payload_len: 2, payload: reply_payload }
    }
    if state.count > 2 && state.keys[2] == observation.payload[0] {
        reply_payload[0] = observation.payload[0]
        reply_payload[1] = state.values[2]
        return KvReply{ status: syscall.SyscallStatus.Ok, payload_len: 2, payload: reply_payload }
    }
    if state.count > 3 && state.keys[3] == observation.payload[0] {
        reply_payload[0] = observation.payload[0]
        reply_payload[1] = state.values[3]
        return KvReply{ status: syscall.SyscallStatus.Ok, payload_len: 2, payload: reply_payload }
    }
    return KvReply{ status: syscall.SyscallStatus.InvalidArgument, payload_len: 0, payload: reply_payload }
}

func kv_on_receive(state: KvServiceState, observation: syscall.ReceiveObservation) KvReceiveResult {
    empty_payload: [4]u8 = ipc.zero_payload()
    if observation.payload_len == 0 {
        return KvReceiveResult{ state: state, reply: KvReply{ status: syscall.SyscallStatus.InvalidArgument, payload_len: 0, payload: empty_payload } }
    }
    if observation.payload_len >= 2 {
        next_state: KvServiceState = kv_on_set(state, observation)
        ok_payload: [4]u8 = ipc.zero_payload()
        return KvReceiveResult{ state: next_state, reply: KvReply{ status: syscall.SyscallStatus.Ok, payload_len: 0, payload: ok_payload } }
    }
    reply: KvReply = kv_on_get(state, observation)
    return KvReceiveResult{ state: state, reply: reply }
}

func debug_kv_count(state: KvServiceState) usize {
    return state.count
}
