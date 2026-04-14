import primitives
import service_effect
import syscall

const KV_CAPACITY: usize = 4

struct KvServiceState {
    pid: u32
    slot: u32
    keys: [4]u8
    values: [4]u8
    count: usize
}

struct KvResult {
    state: KvServiceState
    effect: service_effect.Effect
}

func kv_init(pid: u32, slot: u32) KvServiceState {
    return KvServiceState{ pid: pid, slot: slot, keys: primitives.zero_payload(), values: primitives.zero_payload(), count: 0 }
}

func kvwith(s: KvServiceState, keys: [4]u8, vals: [4]u8, count: usize) KvServiceState {
    return KvServiceState{ pid: s.pid, slot: s.slot, keys: keys, values: vals, count: count }
}

func kvset(s: KvServiceState, m: service_effect.Message) KvServiceState {
    if m.payload_len < 2 {
        return s
    }
    key: u8 = m.payload[0]
    val: u8 = m.payload[1]
    next_keys: [4]u8 = s.keys
    next_vals: [4]u8 = s.values
    for i in 0..s.count {
        if next_keys[i] == key {
            next_vals[i] = val
            return kvwith(s, next_keys, next_vals, s.count)
        }
    }
    if s.count >= KV_CAPACITY {
        // Intentional silent drop: capacity is fixed for bootstrap.
        return s
    }
    next_keys[s.count] = key
    next_vals[s.count] = val
    return kvwith(s, next_keys, next_vals, s.count + 1)
}

func kvget(s: KvServiceState, m: service_effect.Message) service_effect.Effect {
    if m.payload_len == 0 {
        return service_effect.effect_reply(syscall.SyscallStatus.InvalidArgument, 0, primitives.zero_payload())
    }
    key: u8 = m.payload[0]
    for i in 0..s.count {
        if s.keys[i] == key {
            reply: [4]u8 = primitives.zero_payload()
            reply[0] = key
            reply[1] = s.values[i]
            return service_effect.effect_reply(syscall.SyscallStatus.Ok, 2, reply)
        }
    }
    return service_effect.effect_reply(syscall.SyscallStatus.InvalidArgument, 0, primitives.zero_payload())
}

func handle(s: KvServiceState, m: service_effect.Message) KvResult {
    if m.payload_len == 0 {
        return KvResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.InvalidArgument, 0, primitives.zero_payload()) }
    }
    if m.payload_len >= 2 {
        return KvResult{ state: kvset(s, m), effect: service_effect.effect_reply(syscall.SyscallStatus.Ok, 0, primitives.zero_payload()) }
    }
    return KvResult{ state: s, effect: kvget(s, m) }
}

func kvrecv(s: KvServiceState, obs: syscall.ReceiveObservation) KvResult {
    return handle(s, service_effect.from_receive_observation(obs))
}

func kv_count(s: KvServiceState) usize {
    return s.count
}
