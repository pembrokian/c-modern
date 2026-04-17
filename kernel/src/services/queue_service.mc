import primitives
import serial_protocol
import service_effect
import syscall

const QUEUE_CAPACITY: usize = 4

struct QueueServiceState {
    pid: u32
    slot: u32
    items: [QUEUE_CAPACITY]u8
    len: usize
    stall_count: usize
}

struct QueueSnapshot {
    items: [QUEUE_CAPACITY]u8
    len: usize
}

struct QueueResult {
    state: QueueServiceState
    effect: service_effect.Effect
}

func queue_init(pid: u32, slot: u32) QueueServiceState {
    return QueueServiceState{ pid: pid, slot: slot, items: primitives.zero_payload(), len: 0, stall_count: 0 }
}

func queuewith(s: QueueServiceState, items: [QUEUE_CAPACITY]u8, len: usize, stall_count: usize) QueueServiceState {
    return QueueServiceState{ pid: s.pid, slot: s.slot, items: items, len: len, stall_count: stall_count }
}

func queue_snapshot(s: QueueServiceState) QueueSnapshot {
    return QueueSnapshot{ items: s.items, len: s.len }
}

func queue_reload(pid: u32, slot: u32, snap: QueueSnapshot) QueueServiceState {
    return QueueServiceState{ pid: pid, slot: slot, items: snap.items, len: snap.len, stall_count: 0 }
}

func queue_push(s: QueueServiceState, value: u8) QueueServiceState {
    if s.len >= QUEUE_CAPACITY {
        return s
    }
    next_items: [QUEUE_CAPACITY]u8 = s.items
    next_items[s.len] = value
    return queuewith(s, next_items, s.len + 1, 0)
}

func queue_pop(s: QueueServiceState) QueueResult {
    if s.len == 0 {
        return QueueResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.InvalidArgument, 0, primitives.zero_payload()) }
    }
    next_items: [QUEUE_CAPACITY]u8 = primitives.zero_payload()
    for i in 1..s.len {
        next_items[i - 1] = s.items[i]
    }
    reply: [4]u8 = primitives.zero_payload()
    reply[0] = s.items[0]
    return QueueResult{ state: queuewith(s, next_items, s.len - 1, s.stall_count), effect: service_effect.effect_reply(syscall.SyscallStatus.Ok, 1, reply) }
}

func queue_count(s: QueueServiceState) QueueResult {
    return QueueResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.Ok, s.len, primitives.zero_payload()) }
}

func queue_peek(s: QueueServiceState) QueueResult {
    if s.len == 0 {
        return QueueResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.InvalidArgument, 0, primitives.zero_payload()) }
    }
    reply: [4]u8 = primitives.zero_payload()
    reply[0] = s.items[0]
    return QueueResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.Ok, 1, reply) }
}

func handle(s: QueueServiceState, m: service_effect.Message) QueueResult {
    if m.payload_len == 0 {
        return queue_pop(s)
    }
    if m.payload_len == 2 {
        if m.payload[1] != serial_protocol.CMD_BANG {
            return QueueResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.InvalidArgument, 0, primitives.zero_payload()) }
        }
        switch m.payload[0] {
        case serial_protocol.CMD_C:
            return queue_count(s)
        case serial_protocol.CMD_P:
            return queue_peek(s)
        case serial_protocol.CMD_W:
            return QueueResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.Ok, s.stall_count, primitives.zero_payload()) }
        default:
            return QueueResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.InvalidArgument, 0, primitives.zero_payload()) }
        }
    }
    if m.payload_len != 1 {
        return QueueResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.InvalidArgument, 0, primitives.zero_payload()) }
    }
    if s.len >= QUEUE_CAPACITY {
        stalled: QueueServiceState = queuewith(s, s.items, s.len, s.stall_count + 1)
        if s.stall_count > 0 {
            return QueueResult{ state: stalled, effect: service_effect.effect_reply(syscall.SyscallStatus.WouldBlock, 0, primitives.zero_payload()) }
        }
        return QueueResult{ state: stalled, effect: service_effect.effect_reply(syscall.SyscallStatus.Exhausted, 0, primitives.zero_payload()) }
    }
    return QueueResult{ state: queue_push(s, m.payload[0]), effect: service_effect.effect_reply(syscall.SyscallStatus.Ok, 0, primitives.zero_payload()) }
}

func queue_len(s: QueueServiceState) usize {
    return s.len
}

func queue_stall_count(s: QueueServiceState) usize {
    return s.stall_count
}