import primitives
import service_effect
import syscall

const KV_CAPACITY: usize = 4

struct KvServiceState {
    owner_pid: u32
    endpoint_handle_slot: u32
    keys: [4]u8
    values: [4]u8
    count: usize
}

struct KvHandleResult {
    state: KvServiceState
    effect: service_effect.Effect
}

func kv_init(owner_pid: u32, endpoint_handle_slot: u32) KvServiceState {
    return KvServiceState{ owner_pid: owner_pid, endpoint_handle_slot: endpoint_handle_slot, keys: primitives.zero_payload(), values: primitives.zero_payload(), count: 0 }
}

func service_state(owner_pid: u32, endpoint_handle_slot: u32) KvServiceState {
    return kv_init(owner_pid, endpoint_handle_slot)
}

func kv_on_set(state: KvServiceState, msg: service_effect.Message) KvServiceState {
    if msg.payload_len < 2 {
        return state
    }
    next_keys: [4]u8 = state.keys
    next_values: [4]u8 = state.values
    if 0 < state.count && next_keys[0] == msg.payload[0] {
        next_values[0] = msg.payload[1]
        return KvServiceState{ owner_pid: state.owner_pid, endpoint_handle_slot: state.endpoint_handle_slot, keys: next_keys, values: next_values, count: state.count }
    }
    if 1 < state.count && next_keys[1] == msg.payload[0] {
        next_values[1] = msg.payload[1]
        return KvServiceState{ owner_pid: state.owner_pid, endpoint_handle_slot: state.endpoint_handle_slot, keys: next_keys, values: next_values, count: state.count }
    }
    if 2 < state.count && next_keys[2] == msg.payload[0] {
        next_values[2] = msg.payload[1]
        return KvServiceState{ owner_pid: state.owner_pid, endpoint_handle_slot: state.endpoint_handle_slot, keys: next_keys, values: next_values, count: state.count }
    }
    if 3 < state.count && next_keys[3] == msg.payload[0] {
        next_values[3] = msg.payload[1]
        return KvServiceState{ owner_pid: state.owner_pid, endpoint_handle_slot: state.endpoint_handle_slot, keys: next_keys, values: next_values, count: state.count }
    }
    if state.count >= KV_CAPACITY {
        // Intentional silent drop: capacity is fixed for bootstrap.
        // A write to a full store returns the old state unchanged.
        return state
    }
    next_keys[state.count] = msg.payload[0]
    next_values[state.count] = msg.payload[1]
    return KvServiceState{ owner_pid: state.owner_pid, endpoint_handle_slot: state.endpoint_handle_slot, keys: next_keys, values: next_values, count: state.count + 1 }
}

func kv_on_get(state: KvServiceState, msg: service_effect.Message) service_effect.Effect {
    reply_payload: [4]u8 = primitives.zero_payload()
    if msg.payload_len == 0 {
        return service_effect.effect_reply(syscall.SyscallStatus.InvalidArgument, 0, reply_payload)
    }
    if 0 < state.count && state.keys[0] == msg.payload[0] {
        reply_payload[0] = msg.payload[0]
        reply_payload[1] = state.values[0]
        return service_effect.effect_reply(syscall.SyscallStatus.Ok, 2, reply_payload)
    }
    if 1 < state.count && state.keys[1] == msg.payload[0] {
        reply_payload[0] = msg.payload[0]
        reply_payload[1] = state.values[1]
        return service_effect.effect_reply(syscall.SyscallStatus.Ok, 2, reply_payload)
    }
    if 2 < state.count && state.keys[2] == msg.payload[0] {
        reply_payload[0] = msg.payload[0]
        reply_payload[1] = state.values[2]
        return service_effect.effect_reply(syscall.SyscallStatus.Ok, 2, reply_payload)
    }
    if 3 < state.count && state.keys[3] == msg.payload[0] {
        reply_payload[0] = msg.payload[0]
        reply_payload[1] = state.values[3]
        return service_effect.effect_reply(syscall.SyscallStatus.Ok, 2, reply_payload)
    }
    return service_effect.effect_reply(syscall.SyscallStatus.InvalidArgument, 0, reply_payload)
}

func handle(state: KvServiceState, msg: service_effect.Message) KvHandleResult {
    if msg.payload_len == 0 {
        return KvHandleResult{ state: state, effect: service_effect.effect_reply(syscall.SyscallStatus.InvalidArgument, 0, primitives.zero_payload()) }
    }
    if msg.payload_len >= 2 {
        new_state: KvServiceState = kv_on_set(state, msg)
        return KvHandleResult{ state: new_state, effect: service_effect.effect_reply(syscall.SyscallStatus.Ok, 0, primitives.zero_payload()) }
    }
    return KvHandleResult{ state: state, effect: kv_on_get(state, msg) }
}

func kv_on_receive(state: KvServiceState, observation: syscall.ReceiveObservation) KvHandleResult {
    return handle(state, service_effect.from_receive_observation(observation))
}

func debug_kv_count(state: KvServiceState) usize {
    return state.count
}
