import primitives
import service_effect
import syscall

const LOG_BUFFER_CAPACITY: usize = 4

struct LogServiceState {
    pid: u32
    slot: u32
    retained: [LOG_BUFFER_CAPACITY]u8
    len: usize
}

struct LogSnapshot {
    retained: [LOG_BUFFER_CAPACITY]u8
    len: usize
}

struct LogResult {
    state: LogServiceState
    effect: service_effect.Effect
}

func log_init(pid: u32, slot: u32) LogServiceState {
    return LogServiceState{ pid: pid, slot: slot, retained: primitives.zero_payload(), len: 0 }
}

func logwith(s: LogServiceState, retained: [LOG_BUFFER_CAPACITY]u8, len: usize) LogServiceState {
    return LogServiceState{ pid: s.pid, slot: s.slot, retained: retained, len: len }
}

func log_snapshot(s: LogServiceState) LogSnapshot {
    return LogSnapshot{ retained: s.retained, len: s.len }
}

func log_reload(pid: u32, slot: u32, snap: LogSnapshot) LogServiceState {
    return LogServiceState{ pid: pid, slot: slot, retained: snap.retained, len: snap.len }
}

func log_append(s: LogServiceState, obs: syscall.ReceiveObservation) LogServiceState {
    if obs.payload_len == 0 {
        return s
    }
    if s.len >= LOG_BUFFER_CAPACITY {
        return s
    }
    next_retained: [LOG_BUFFER_CAPACITY]u8 = s.retained
    next_retained[s.len] = obs.payload[0]
    return logwith(s, next_retained, s.len + 1)
}

func log_tail(s: LogServiceState) service_effect.Effect {
    payload: [LOG_BUFFER_CAPACITY]u8 = primitives.zero_payload()
    for i in 0..s.len {
        payload[i] = s.retained[i]
    }
    return service_effect.effect_reply(syscall.SyscallStatus.Ok, s.len, payload)
}

func handle(s: LogServiceState, m: service_effect.Message) LogResult {
    if m.payload_len == 0 {
        return LogResult{ state: s, effect: log_tail(s) }
    }
    // Backpressure: full buffer is visible to the caller as Exhausted.
    if s.len >= LOG_BUFFER_CAPACITY {
        return LogResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.Exhausted, 0, primitives.zero_payload()) }
    }
    new_state: LogServiceState = log_append(s, syscall.ReceiveObservation{ status: syscall.SyscallStatus.Ok, block_reason: syscall.BlockReason.None, endpoint_id: m.endpoint_id, source_pid: m.source_pid, payload_len: m.payload_len, received_handle_slot: 0, received_handle_count: 0, payload: m.payload })
    return LogResult{ state: new_state, effect: service_effect.effect_reply(syscall.SyscallStatus.Ok, 0, primitives.zero_payload()) }
}

func log_len(s: LogServiceState) usize {
    return s.len
}