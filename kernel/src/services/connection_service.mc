import primitives
import service_effect
import syscall

const CONNECTION_CAPACITY: usize = 4

const CONNECTION_OP_OPEN: u8 = 79   // 'O'
const CONNECTION_OP_RECEIVE: u8 = 82  // 'R'
const CONNECTION_OP_SEND: u8 = 83   // 'S'
const CONNECTION_OP_CLOSE: u8 = 67  // 'C'
const CONNECTION_OP_EXECUTE: u8 = 88  // 'X'
const CONNECTION_EXTERNAL_TICKET_BASE: u8 = 128

const CONNECTION_STATE_INVALID: u8 = 73       // 'I'
const CONNECTION_STATE_OPEN: u8 = 79          // 'O'
const CONNECTION_STATE_RECEIVED: u8 = 82      // 'R'
const CONNECTION_STATE_REPLIED: u8 = 83       // 'S'
const CONNECTION_STATE_DISCONNECTED: u8 = 68  // 'D'
const CONNECTION_STATE_TIMED_OUT: u8 = 84     // 'T'

const CONNECTION_CLOSE_NORMAL: u8 = 78        // 'N'
const CONNECTION_CLOSE_DISCONNECT: u8 = 68    // 'D'
const CONNECTION_CLOSE_TIMEOUT: u8 = 84       // 'T'

struct ConnectionServiceState {
    pid: u32
    slot: u32
    ids: [CONNECTION_CAPACITY]u8
    state: [CONNECTION_CAPACITY]u8
    ingress: [CONNECTION_CAPACITY]u8
    reply: [CONNECTION_CAPACITY]u8
    work: [CONNECTION_CAPACITY]u8
    idle: [CONNECTION_CAPACITY]u8
    next: u8
}

struct ConnectionResult {
    state: ConnectionServiceState
    effect: service_effect.Effect
}

struct ConnectionExecuteResult {
    state: ConnectionServiceState
    effect: service_effect.Effect
    request: u8
    opcode: u8
}

func connection_init(pid: u32, slot: u32) ConnectionServiceState {
    return ConnectionServiceState{
        pid: pid,
        slot: slot,
        ids: primitives.zero_payload(),
        state: primitives.zero_payload(),
        ingress: primitives.zero_payload(),
        reply: primitives.zero_payload(),
        work: primitives.zero_payload(),
        idle: primitives.zero_payload()
        , next: 1
    }
}

func connectionwith(s: ConnectionServiceState, ids: [CONNECTION_CAPACITY]u8, state: [CONNECTION_CAPACITY]u8, ingress: [CONNECTION_CAPACITY]u8, reply: [CONNECTION_CAPACITY]u8, work: [CONNECTION_CAPACITY]u8, idle: [CONNECTION_CAPACITY]u8, next: u8) ConnectionServiceState {
    return ConnectionServiceState{ pid: s.pid, slot: s.slot, ids: ids, state: state, ingress: ingress, reply: reply, work: work, idle: idle, next: next }
}

func connection_reply(status: syscall.SyscallStatus, len: usize, payload0: u8, payload1: u8, payload2: u8, payload3: u8) service_effect.Effect {
    payload: [4]u8 = primitives.zero_payload()
    if len > 0 {
        payload[0] = payload0
    }
    if len > 1 {
        payload[1] = payload1
    }
    if len > 2 {
        payload[2] = payload2
    }
    if len > 3 {
        payload[3] = payload3
    }
    return service_effect.effect_reply(status, len, payload)
}

func connection_find(slot: u8) usize {
    return usize(slot)
}

func connection_slot_valid(idx: usize) bool {
    return idx < CONNECTION_CAPACITY
}

func connection_mark_timed_out(s: ConnectionServiceState, idx: usize) ConnectionServiceState {
    next_state: [CONNECTION_CAPACITY]u8 = s.state
    next_idle: [CONNECTION_CAPACITY]u8 = s.idle
    next_work: [CONNECTION_CAPACITY]u8 = s.work
    next_state[idx] = CONNECTION_STATE_TIMED_OUT
    next_idle[idx] = 0
    next_work[idx] = 0
    return connectionwith(s, s.ids, next_state, s.ingress, s.reply, next_work, next_idle, s.next)
}

func connection_active_state(state: u8) bool {
    return state == CONNECTION_STATE_OPEN || state == CONNECTION_STATE_RECEIVED || state == CONNECTION_STATE_REPLIED
}

func connection_validate(s: ConnectionServiceState, idx: usize) ConnectionResult {
    if !connection_slot_valid(idx) {
        return ConnectionResult{ state: s, effect: connection_reply(syscall.SyscallStatus.InvalidArgument, 1, CONNECTION_STATE_INVALID, 0, 0, 0) }
    }

    if s.state[idx] == CONNECTION_STATE_TIMED_OUT {
        return ConnectionResult{ state: s, effect: connection_reply(syscall.SyscallStatus.WouldBlock, 1, CONNECTION_STATE_TIMED_OUT, 0, 0, 0) }
    }

    if s.state[idx] == CONNECTION_STATE_DISCONNECTED {
        return ConnectionResult{ state: s, effect: connection_reply(syscall.SyscallStatus.Closed, 1, CONNECTION_STATE_DISCONNECTED, 0, 0, 0) }
    }

    if !connection_active_state(s.state[idx]) {
        return ConnectionResult{ state: s, effect: connection_reply(syscall.SyscallStatus.InvalidArgument, 1, CONNECTION_STATE_INVALID, 0, 0, 0) }
    }

    if s.idle[idx] == 0 {
        timed := connection_mark_timed_out(s, idx)
        return ConnectionResult{ state: timed, effect: connection_reply(syscall.SyscallStatus.WouldBlock, 1, CONNECTION_STATE_TIMED_OUT, 0, 0, 0) }
    }

    return ConnectionResult{ state: s, effect: service_effect.effect_none() }
}

func connection_open(s: ConnectionServiceState, slot: u8, budget: u8) ConnectionResult {
    idx := connection_find(slot)
    if !connection_slot_valid(idx) {
        return ConnectionResult{ state: s, effect: connection_reply(syscall.SyscallStatus.InvalidArgument, 1, CONNECTION_STATE_INVALID, 0, 0, 0) }
    }
    if budget == 0 {
        return ConnectionResult{ state: s, effect: connection_reply(syscall.SyscallStatus.InvalidArgument, 1, CONNECTION_STATE_TIMED_OUT, 0, 0, 0) }
    }

    next_ids: [CONNECTION_CAPACITY]u8 = s.ids
    next_state: [CONNECTION_CAPACITY]u8 = s.state
    next_ingress: [CONNECTION_CAPACITY]u8 = s.ingress
    next_reply: [CONNECTION_CAPACITY]u8 = s.reply
    next_work: [CONNECTION_CAPACITY]u8 = s.work
    next_idle: [CONNECTION_CAPACITY]u8 = s.idle

    next_ids[idx] = slot
    next_state[idx] = CONNECTION_STATE_OPEN
    next_ingress[idx] = 0
    next_reply[idx] = 0
    next_work[idx] = 0
    next_idle[idx] = budget

    next := connectionwith(s, next_ids, next_state, next_ingress, next_reply, next_work, next_idle, s.next)
    return ConnectionResult{ state: next, effect: connection_reply(syscall.SyscallStatus.Ok, 2, slot, CONNECTION_STATE_OPEN, 0, 0) }
}

func connection_receive(s: ConnectionServiceState, slot: u8, value: u8) ConnectionResult {
    idx := connection_find(slot)
    checked := connection_validate(s, idx)
    if service_effect.effect_has_reply(checked.effect) == 1 {
        return checked
    }

    next_state: [CONNECTION_CAPACITY]u8 = checked.state.state
    next_ingress: [CONNECTION_CAPACITY]u8 = checked.state.ingress
    next_work: [CONNECTION_CAPACITY]u8 = checked.state.work
    next_idle: [CONNECTION_CAPACITY]u8 = checked.state.idle

    if next_work[idx] != 0 {
        return ConnectionResult{ state: checked.state, effect: connection_reply(syscall.SyscallStatus.InvalidArgument, 1, checked.state.state[idx], 0, 0, 0) }
    }

    next_state[idx] = CONNECTION_STATE_RECEIVED
    next_ingress[idx] = value
    next_idle[idx] = next_idle[idx] - 1

    next := connectionwith(checked.state, checked.state.ids, next_state, next_ingress, checked.state.reply, next_work, next_idle, checked.state.next)
    return ConnectionResult{ state: next, effect: connection_reply(syscall.SyscallStatus.Ok, 3, slot, value, CONNECTION_STATE_RECEIVED, 0) }
}

func connection_send(s: ConnectionServiceState, slot: u8, value: u8) ConnectionResult {
    idx := connection_find(slot)
    checked := connection_validate(s, idx)
    if service_effect.effect_has_reply(checked.effect) == 1 {
        return checked
    }
    if checked.state.state[idx] != CONNECTION_STATE_RECEIVED && checked.state.state[idx] != CONNECTION_STATE_REPLIED {
        return ConnectionResult{ state: checked.state, effect: connection_reply(syscall.SyscallStatus.InvalidArgument, 1, checked.state.state[idx], 0, 0, 0) }
    }
    if checked.state.work[idx] != 0 {
        return ConnectionResult{ state: checked.state, effect: connection_reply(syscall.SyscallStatus.InvalidArgument, 1, checked.state.state[idx], 0, 0, 0) }
    }

    next_state: [CONNECTION_CAPACITY]u8 = checked.state.state
    next_reply: [CONNECTION_CAPACITY]u8 = checked.state.reply
    next_work: [CONNECTION_CAPACITY]u8 = checked.state.work
    next_idle: [CONNECTION_CAPACITY]u8 = checked.state.idle

    next_state[idx] = CONNECTION_STATE_REPLIED
    next_reply[idx] = value
    next_idle[idx] = next_idle[idx] - 1

    next := connectionwith(checked.state, checked.state.ids, next_state, checked.state.ingress, next_reply, next_work, next_idle, checked.state.next)
    return ConnectionResult{ state: next, effect: connection_reply(syscall.SyscallStatus.Ok, 4, slot, checked.state.ingress[idx], value, CONNECTION_STATE_REPLIED) }
}

func connection_close(s: ConnectionServiceState, slot: u8, reason: u8) ConnectionResult {
    idx := connection_find(slot)
    if !connection_slot_valid(idx) {
        return ConnectionResult{ state: s, effect: connection_reply(syscall.SyscallStatus.InvalidArgument, 1, CONNECTION_STATE_INVALID, 0, 0, 0) }
    }

    next_state: [CONNECTION_CAPACITY]u8 = s.state
    next_work: [CONNECTION_CAPACITY]u8 = s.work
    next_idle: [CONNECTION_CAPACITY]u8 = s.idle

    switch reason {
    case CONNECTION_CLOSE_DISCONNECT:
        next_state[idx] = CONNECTION_STATE_DISCONNECTED
    case CONNECTION_CLOSE_TIMEOUT:
        next_state[idx] = CONNECTION_STATE_TIMED_OUT
    default:
        next_state[idx] = CONNECTION_STATE_INVALID
    }

    next_work[idx] = 0
    next_idle[idx] = 0
    next := connectionwith(s, s.ids, next_state, s.ingress, s.reply, next_work, next_idle, s.next)
    return ConnectionResult{ state: next, effect: connection_reply(syscall.SyscallStatus.Ok, 2, slot, next_state[idx], 0, 0) }
}

func connection_next_request_id(next: u8) u8 {
    if next == 0 {
        return 1
    }
    return next
}

func connection_execute_prepare(s: ConnectionServiceState, slot: u8) ConnectionExecuteResult {
    idx := connection_find(slot)
    checked := connection_validate(s, idx)
    if service_effect.effect_has_reply(checked.effect) == 1 {
        return ConnectionExecuteResult{ state: checked.state, effect: checked.effect, request: 0, opcode: 0 }
    }
    if checked.state.state[idx] != CONNECTION_STATE_RECEIVED {
        return ConnectionExecuteResult{ state: checked.state, effect: connection_reply(syscall.SyscallStatus.InvalidArgument, 1, checked.state.state[idx], 0, 0, 0), request: 0, opcode: 0 }
    }
    if checked.state.work[idx] != 0 {
        return ConnectionExecuteResult{ state: checked.state, effect: connection_reply(syscall.SyscallStatus.InvalidArgument, 1, checked.state.work[idx], 0, 0, 0), request: 0, opcode: 0 }
    }

    next_work: [CONNECTION_CAPACITY]u8 = checked.state.work
    next_idle: [CONNECTION_CAPACITY]u8 = checked.state.idle
    request := connection_next_request_id(checked.state.next)
    next_work[idx] = request
    next_idle[idx] = next_idle[idx] - 1

    next := connectionwith(checked.state, checked.state.ids, checked.state.state, checked.state.ingress, checked.state.reply, next_work, next_idle, connection_next_request_id(request + 1))
    return ConnectionExecuteResult{ state: next, effect: service_effect.effect_none(), request: request, opcode: checked.state.ingress[idx] }
}

func connection_request_active(s: ConnectionServiceState, slot: u8, request: u8) bool {
    idx := connection_find(slot)
    if !connection_slot_valid(idx) {
        return false
    }
    if !connection_active_state(s.state[idx]) {
        return false
    }
    return s.work[idx] == request
}

func connection_request_finish(s: ConnectionServiceState, slot: u8, request: u8) ConnectionServiceState {
    idx := connection_find(slot)
    if !connection_slot_valid(idx) {
        return s
    }
    if s.work[idx] != request {
        return s
    }
    next_work: [CONNECTION_CAPACITY]u8 = s.work
    next_work[idx] = 0
    return connectionwith(s, s.ids, s.state, s.ingress, s.reply, next_work, s.idle, s.next)
}

func handle(s: ConnectionServiceState, m: service_effect.Message) ConnectionResult {
    if m.payload_len == 2 && m.payload[0] == CONNECTION_OP_EXECUTE {
        prepared := connection_execute_prepare(s, m.payload[1])
        return ConnectionResult{ state: prepared.state, effect: prepared.effect }
    }
    if m.payload_len != 3 {
        return ConnectionResult{ state: s, effect: connection_reply(syscall.SyscallStatus.InvalidArgument, 1, CONNECTION_STATE_INVALID, 0, 0, 0) }
    }

    switch m.payload[0] {
    case CONNECTION_OP_OPEN:
        return connection_open(s, m.payload[1], m.payload[2])
    case CONNECTION_OP_RECEIVE:
        return connection_receive(s, m.payload[1], m.payload[2])
    case CONNECTION_OP_SEND:
        return connection_send(s, m.payload[1], m.payload[2])
    case CONNECTION_OP_CLOSE:
        return connection_close(s, m.payload[1], m.payload[2])
    case CONNECTION_OP_EXECUTE:
        return ConnectionResult{ state: s, effect: connection_reply(syscall.SyscallStatus.InvalidArgument, 1, CONNECTION_STATE_INVALID, 0, 0, 0) }
    default:
        return ConnectionResult{ state: s, effect: connection_reply(syscall.SyscallStatus.InvalidArgument, 1, CONNECTION_STATE_INVALID, 0, 0, 0) }
    }
}

func connection_active_count(s: ConnectionServiceState) usize {
    count: usize = 0
    for i in 0..CONNECTION_CAPACITY {
        if connection_active_state(s.state[i]) {
            count = count + 1
        }
    }
    return count
}

func connection_state_code(s: ConnectionServiceState, slot: u8) u8 {
    idx := connection_find(slot)
    if !connection_slot_valid(idx) {
        return CONNECTION_STATE_INVALID
    }
    if s.state[idx] == 0 {
        return CONNECTION_STATE_INVALID
    }
    return s.state[idx]
}