import primitives
import serial_protocol
import service_effect
import syscall

const MAILBOX_CAPACITY: usize = 4

struct CompletionMailboxServiceState {
    pid: u32
    slot: u32
    ids: [MAILBOX_CAPACITY]u8
    states: [MAILBOX_CAPACITY]u8
    restarts: [MAILBOX_CAPACITY]u8
    gens: [MAILBOX_CAPACITY]u8
    len: usize
    stall_count: usize
}

struct CompletionMailboxSnapshot {
    ids: [MAILBOX_CAPACITY]u8
    states: [MAILBOX_CAPACITY]u8
    restarts: [MAILBOX_CAPACITY]u8
    gens: [MAILBOX_CAPACITY]u8
    len: usize
}

struct CompletionMailboxResult {
    state: CompletionMailboxServiceState
    effect: service_effect.Effect
}

func completion_mailbox_init(pid: u32, slot: u32) CompletionMailboxServiceState {
    return CompletionMailboxServiceState{ pid: pid, slot: slot, ids: primitives.zero_payload(), states: primitives.zero_payload(), restarts: primitives.zero_payload(), gens: primitives.zero_payload(), len: 0, stall_count: 0 }
}

func mailboxwith(s: CompletionMailboxServiceState, ids: [MAILBOX_CAPACITY]u8, states: [MAILBOX_CAPACITY]u8, restarts: [MAILBOX_CAPACITY]u8, gens: [MAILBOX_CAPACITY]u8, len: usize, stall_count: usize) CompletionMailboxServiceState {
    return CompletionMailboxServiceState{ pid: s.pid, slot: s.slot, ids: ids, states: states, restarts: restarts, gens: gens, len: len, stall_count: stall_count }
}

func completion_mailbox_snapshot(s: CompletionMailboxServiceState) CompletionMailboxSnapshot {
    return CompletionMailboxSnapshot{ ids: s.ids, states: s.states, restarts: s.restarts, gens: s.gens, len: s.len }
}

func completion_mailbox_reload(pid: u32, slot: u32, snap: CompletionMailboxSnapshot) CompletionMailboxServiceState {
    return CompletionMailboxServiceState{ pid: pid, slot: slot, ids: snap.ids, states: snap.states, restarts: snap.restarts, gens: snap.gens, len: snap.len, stall_count: 0 }
}

func completion_mailbox_enqueue(s: CompletionMailboxServiceState, id: u8, state: u8, restart: u8, generation: u8) CompletionMailboxResult {
    if s.len >= MAILBOX_CAPACITY {
        stalled := mailboxwith(s, s.ids, s.states, s.restarts, s.gens, s.len, s.stall_count + 1)
        if s.stall_count > 0 {
            return CompletionMailboxResult{ state: stalled, effect: service_effect.effect_reply(syscall.SyscallStatus.WouldBlock, 0, primitives.zero_payload()) }
        }
        return CompletionMailboxResult{ state: stalled, effect: service_effect.effect_reply(syscall.SyscallStatus.Exhausted, 0, primitives.zero_payload()) }
    }

    ids := s.ids
    states := s.states
    restarts := s.restarts
    gens := s.gens
    ids[s.len] = id
    states[s.len] = state
    restarts[s.len] = restart
    gens[s.len] = generation
    next := mailboxwith(s, ids, states, restarts, gens, s.len + 1, 0)
    return CompletionMailboxResult{ state: next, effect: service_effect.effect_reply(syscall.SyscallStatus.Ok, 0, primitives.zero_payload()) }
}

func completion_mailbox_fetch(s: CompletionMailboxServiceState) CompletionMailboxResult {
    if s.len == 0 {
        return CompletionMailboxResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.InvalidArgument, 0, primitives.zero_payload()) }
    }
    payload: [4]u8 = primitives.zero_payload()
    payload[0] = s.ids[0]
    payload[1] = s.states[0]
    payload[2] = s.restarts[0]
    payload[3] = s.gens[0]
    return CompletionMailboxResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.Ok, 4, payload) }
}

func completion_mailbox_ack(s: CompletionMailboxServiceState) CompletionMailboxResult {
    if s.len == 0 {
        return CompletionMailboxResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.InvalidArgument, 0, primitives.zero_payload()) }
    }

    ids: [MAILBOX_CAPACITY]u8 = primitives.zero_payload()
    states: [MAILBOX_CAPACITY]u8 = primitives.zero_payload()
    restarts: [MAILBOX_CAPACITY]u8 = primitives.zero_payload()
    gens: [MAILBOX_CAPACITY]u8 = primitives.zero_payload()
    for i in 1..s.len {
        ids[i - 1] = s.ids[i]
        states[i - 1] = s.states[i]
        restarts[i - 1] = s.restarts[i]
        gens[i - 1] = s.gens[i]
    }

    next := mailboxwith(s, ids, states, restarts, gens, s.len - 1, s.stall_count)
    return CompletionMailboxResult{ state: next, effect: service_effect.effect_reply(syscall.SyscallStatus.Ok, 0, primitives.zero_payload()) }
}

func completion_mailbox_count(s: CompletionMailboxServiceState) CompletionMailboxResult {
    return CompletionMailboxResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.Ok, s.len, primitives.zero_payload()) }
}

func handle(s: CompletionMailboxServiceState, m: service_effect.Message) CompletionMailboxResult {
    if m.payload_len != 2 || m.payload[1] != serial_protocol.CMD_BANG {
        return CompletionMailboxResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.InvalidArgument, 0, primitives.zero_payload()) }
    }

    switch m.payload[0] {
    case serial_protocol.CMD_F:
        return completion_mailbox_fetch(s)
    case serial_protocol.CMD_A:
        return completion_mailbox_ack(s)
    case serial_protocol.CMD_C:
        return completion_mailbox_count(s)
    default:
        return CompletionMailboxResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.InvalidArgument, 0, primitives.zero_payload()) }
    }
}

func completion_backlog_count(s: CompletionMailboxServiceState) usize {
    return s.len
}

func completion_stall_count(s: CompletionMailboxServiceState) usize {
    return s.stall_count
}