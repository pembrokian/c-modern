import primitives
import service_effect
import syscall

const TICKET_CAPACITY: usize = 4
const TICKET_STALE: u8 = 83  // 'S'
const TICKET_CONSUMED: u8 = 67  // 'C'
const TICKET_INVALID: u8 = 73  // 'I'

struct TicketServiceState {
    pid: u32
    slot: u32
    epoch: u8
    next: u8
    ids: [TICKET_CAPACITY]u8
    len: usize
}

struct TicketResult {
    state: TicketServiceState
    effect: service_effect.Effect
}

func ticket_init(pid: u32, slot: u32, epoch: u8) TicketServiceState {
    return TicketServiceState{ pid: pid, slot: slot, epoch: epoch, next: 1, ids: primitives.zero_payload(), len: 0 }
}

func ticketwith(s: TicketServiceState, next: u8, ids: [TICKET_CAPACITY]u8, len: usize) TicketServiceState {
    return TicketServiceState{ pid: s.pid, slot: s.slot, epoch: s.epoch, next: next, ids: ids, len: len }
}

func ticket_failure(status: syscall.SyscallStatus, code: u8) service_effect.Effect {
    payload: [4]u8 = primitives.zero_payload()
    payload[0] = code
    return service_effect.effect_reply(status, 1, payload)
}

func ticket_issue(s: TicketServiceState) TicketResult {
    if s.len >= TICKET_CAPACITY {
        return TicketResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.Exhausted, 0, primitives.zero_payload()) }
    }
    ids: [TICKET_CAPACITY]u8 = s.ids
    ids[s.len] = s.next
    payload: [4]u8 = primitives.zero_payload()
    payload[0] = s.epoch
    payload[1] = s.next
    return TicketResult{ state: ticketwith(s, s.next + 1, ids, s.len + 1), effect: service_effect.effect_reply(syscall.SyscallStatus.Ok, 2, payload) }
}

func ticket_use(s: TicketServiceState, epoch: u8, id: u8) TicketResult {
    if epoch != s.epoch {
        return TicketResult{ state: s, effect: ticket_failure(syscall.SyscallStatus.InvalidArgument, TICKET_STALE) }
    }
    for i in 0..s.len {
        if s.ids[i] == id {
            ids: [TICKET_CAPACITY]u8 = primitives.zero_payload()
            for keep in 0..i {
                ids[keep] = s.ids[keep]
            }
            for keep in i + 1..s.len {
                ids[keep - 1] = s.ids[keep]
            }
            payload: [4]u8 = primitives.zero_payload()
            payload[0] = id
            return TicketResult{ state: ticketwith(s, s.next, ids, s.len - 1), effect: service_effect.effect_reply(syscall.SyscallStatus.Ok, 1, payload) }
        }
    }

    if id != 0 && id < s.next {
        return TicketResult{ state: s, effect: ticket_failure(syscall.SyscallStatus.InvalidArgument, TICKET_CONSUMED) }
    }
    return TicketResult{ state: s, effect: ticket_failure(syscall.SyscallStatus.InvalidArgument, TICKET_INVALID) }
}

func handle(s: TicketServiceState, m: service_effect.Message) TicketResult {
    if m.payload_len == 0 {
        return ticket_issue(s)
    }
    if m.payload_len != 2 {
        return TicketResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.InvalidArgument, 0, primitives.zero_payload()) }
    }
    return ticket_use(s, m.payload[0], m.payload[1])
}

func ticket_len(s: TicketServiceState) usize {
    return s.len
}