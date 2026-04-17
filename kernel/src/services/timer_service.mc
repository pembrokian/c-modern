import primitives
import service_effect
import syscall

const TIMER_CAPACITY: usize = 4

const TIMER_OP_CREATE: u8 = 67   // 'C'
const TIMER_OP_CANCEL: u8 = 88   // 'X'
const TIMER_OP_QUERY: u8 = 81    // 'Q'
const TIMER_OP_EXPIRED: u8 = 69  // 'E'

const TIMER_STATE_NONE: u8 = 78       // 'N'
const TIMER_STATE_ACTIVE: u8 = 65     // 'A'
const TIMER_STATE_EXPIRED: u8 = 69    // 'E'
const TIMER_STATE_CANCELLED: u8 = 67  // 'C'

struct TimerServiceState {
    pid: u32
    slot: u32
    ids: [TIMER_CAPACITY]u8
    due: [TIMER_CAPACITY]u8
    state: [TIMER_CAPACITY]u8
    len: usize
    tick: u8
}

struct TimerSnapshot {
    ids: [TIMER_CAPACITY]u8
    due: [TIMER_CAPACITY]u8
    state: [TIMER_CAPACITY]u8
    len: usize
    tick: u8
}

struct TimerResult {
    state: TimerServiceState
    effect: service_effect.Effect
}

func timer_init(pid: u32, slot: u32) TimerServiceState {
    return TimerServiceState{ pid: pid, slot: slot, ids: primitives.zero_payload(), due: primitives.zero_payload(), state: primitives.zero_payload(), len: 0, tick: 0 }
}

func timerwith(s: TimerServiceState, ids: [TIMER_CAPACITY]u8, due: [TIMER_CAPACITY]u8, state: [TIMER_CAPACITY]u8, len: usize, tick: u8) TimerServiceState {
    return TimerServiceState{ pid: s.pid, slot: s.slot, ids: ids, due: due, state: state, len: len, tick: tick }
}

func timer_snapshot(s: TimerServiceState) TimerSnapshot {
    return TimerSnapshot{ ids: s.ids, due: s.due, state: s.state, len: s.len, tick: s.tick }
}

func timer_reload(pid: u32, slot: u32, snap: TimerSnapshot) TimerServiceState {
    return TimerServiceState{ pid: pid, slot: slot, ids: snap.ids, due: snap.due, state: snap.state, len: snap.len, tick: snap.tick }
}

func timer_find(s: TimerServiceState, id: u8) usize {
    for i in 0..s.len {
        if s.ids[i] == id {
            return i
        }
    }
    return TIMER_CAPACITY
}

func timer_tick(s: TimerServiceState) TimerServiceState {
    next_due: [TIMER_CAPACITY]u8 = s.due
    next_state: [TIMER_CAPACITY]u8 = s.state
    for i in 0..s.len {
        if s.state[i] != TIMER_STATE_ACTIVE {
            continue
        }
        if s.due[i] == 0 {
            next_state[i] = TIMER_STATE_EXPIRED
            continue
        }
        next_due[i] = s.due[i] - 1
        if next_due[i] == 0 {
            next_state[i] = TIMER_STATE_EXPIRED
        }
    }
    return timerwith(s, s.ids, next_due, next_state, s.len, s.tick + 1)
}

func timer_reply_status(status: syscall.SyscallStatus, code: u8) service_effect.Effect {
    payload: [4]u8 = primitives.zero_payload()
    if code != 0 {
        payload[0] = code
        return service_effect.effect_reply(status, 1, payload)
    }
    return service_effect.effect_reply(status, 0, payload)
}

func timer_create(s: TimerServiceState, id: u8, due_tick: u8) TimerResult {
    next_ids: [TIMER_CAPACITY]u8 = s.ids
    next_due: [TIMER_CAPACITY]u8 = s.due
    next_state: [TIMER_CAPACITY]u8 = s.state

    idx := timer_find(s, id)
    if idx < TIMER_CAPACITY {
        next_ids[idx] = id
        next_due[idx] = due_tick
        next_state[idx] = TIMER_STATE_ACTIVE
        return TimerResult{ state: timerwith(s, next_ids, next_due, next_state, s.len, s.tick), effect: timer_reply_status(syscall.SyscallStatus.Ok, 0) }
    }

    if s.len >= TIMER_CAPACITY {
        return TimerResult{ state: s, effect: timer_reply_status(syscall.SyscallStatus.Exhausted, 0) }
    }

    next_ids[s.len] = id
    next_due[s.len] = due_tick
    next_state[s.len] = TIMER_STATE_ACTIVE
    return TimerResult{ state: timerwith(s, next_ids, next_due, next_state, s.len + 1, s.tick), effect: timer_reply_status(syscall.SyscallStatus.Ok, 0) }
}

func timer_cancel(s: TimerServiceState, id: u8) TimerResult {
    idx := timer_find(s, id)
    if idx >= TIMER_CAPACITY {
        return TimerResult{ state: s, effect: timer_reply_status(syscall.SyscallStatus.InvalidArgument, 0) }
    }
    if s.state[idx] == TIMER_STATE_ACTIVE {
        next_state: [TIMER_CAPACITY]u8 = s.state
        next_state[idx] = TIMER_STATE_CANCELLED
        return TimerResult{ state: timerwith(s, s.ids, s.due, next_state, s.len, s.tick), effect: timer_reply_status(syscall.SyscallStatus.Ok, 0) }
    }
    return TimerResult{ state: s, effect: timer_reply_status(syscall.SyscallStatus.InvalidArgument, s.state[idx]) }
}

func timer_query(s: TimerServiceState, id: u8) TimerResult {
    idx := timer_find(s, id)
    if idx >= TIMER_CAPACITY {
        return TimerResult{ state: s, effect: timer_reply_status(syscall.SyscallStatus.InvalidArgument, 0) }
    }
    payload: [4]u8 = primitives.zero_payload()
    payload[0] = s.state[idx]
    payload[1] = s.due[idx]
    return TimerResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.Ok, 2, payload) }
}

func timer_expired(s: TimerServiceState, window: u8) TimerResult {
    payload: [4]u8 = primitives.zero_payload()
    limit: usize = usize(window)
    if limit > TIMER_CAPACITY {
        limit = TIMER_CAPACITY
    }
    count: usize = 0
    for i in 0..s.len {
        if s.state[i] != TIMER_STATE_EXPIRED {
            continue
        }
        if count >= limit {
            break
        }
        payload[count] = s.ids[i]
        count = count + 1
    }
    return TimerResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.Ok, count, payload) }
}

func handle(s: TimerServiceState, m: service_effect.Message) TimerResult {
    step: TimerServiceState = timer_tick(s)

    if m.payload_len < 2 {
        return TimerResult{ state: step, effect: timer_reply_status(syscall.SyscallStatus.InvalidArgument, 0) }
    }

    op: u8 = m.payload[0]
    switch op {
    case TIMER_OP_CREATE:
        if m.payload_len != 3 {
            return TimerResult{ state: step, effect: timer_reply_status(syscall.SyscallStatus.InvalidArgument, 0) }
        }
        return timer_create(step, m.payload[1], m.payload[2])
    case TIMER_OP_CANCEL:
        if m.payload_len != 2 {
            return TimerResult{ state: step, effect: timer_reply_status(syscall.SyscallStatus.InvalidArgument, 0) }
        }
        return timer_cancel(step, m.payload[1])
    case TIMER_OP_QUERY:
        if m.payload_len != 2 {
            return TimerResult{ state: step, effect: timer_reply_status(syscall.SyscallStatus.InvalidArgument, 0) }
        }
        return timer_query(step, m.payload[1])
    case TIMER_OP_EXPIRED:
        if m.payload_len != 2 {
            return TimerResult{ state: step, effect: timer_reply_status(syscall.SyscallStatus.InvalidArgument, 0) }
        }
        return timer_expired(step, m.payload[1])
    default:
        return TimerResult{ state: step, effect: timer_reply_status(syscall.SyscallStatus.InvalidArgument, 0) }
    }
}

func timer_active_count(s: TimerServiceState) usize {
    count: usize = 0
    for i in 0..s.len {
        if s.state[i] == TIMER_STATE_ACTIVE {
            count = count + 1
        }
    }
    return count
}
