import object_store_service
import primitives
import service_effect
import syscall

const LEASE_CAPACITY: usize = 4

const LEASE_CMD_ISSUE_COMPLETION: u8 = 73       // 'I'
const LEASE_CMD_CONSUME_COMPLETION: u8 = 85     // 'U'
const LEASE_CMD_ISSUE_INSTALLER_APPLY: u8 = 65  // 'A'
const LEASE_CMD_ISSUE_OBJECT_UPDATE: u8 = 87    // 'W'
const LEASE_CMD_CONSUME_OBJECT_UPDATE: u8 = 68  // 'D'
const LEASE_CMD_ISSUE_EXTERNAL_TICKET: u8 = 84  // 'T'

const LEASE_KIND_COMPLETION: u8 = 67     // 'C'
const LEASE_KIND_INSTALLER_APPLY: u8 = 65  // 'A'
const LEASE_KIND_OBJECT_UPDATE: u8 = 87  // 'W'
const LEASE_KIND_EXTERNAL_TICKET: u8 = 84  // 'T'

const LEASE_OP_NONE: u8 = 0
const LEASE_OP_TAKE_COMPLETION: u8 = 1
const LEASE_OP_SCHEDULE_OBJECT_UPDATE: u8 = 2
const LEASE_OP_USE_EXTERNAL_TICKET: u8 = 3
const LEASE_OP_USE_INSTALLER_APPLY: u8 = 4

const LEASE_STALE: u8 = 83    // 'S'
const LEASE_INVALID: u8 = 73  // 'I'
const LEASE_CONSUMED: u8 = 80 // 'P'

struct LeaseServiceState {
    pid: u32
    slot: u32
    next: u8
    ids: [LEASE_CAPACITY]u8
    kinds: [LEASE_CAPACITY]u8
    firsts: [LEASE_CAPACITY]u8
    seconds: [LEASE_CAPACITY]u8
    thirds: [LEASE_CAPACITY]u8
    gens: [LEASE_CAPACITY]u8
    len: usize
}

struct LeaseResult {
    state: LeaseServiceState
    effect: service_effect.Effect
    op: u8
    first: u8
    second: u8
    third: u8
}

func lease_init(pid: u32, slot: u32) LeaseServiceState {
    return LeaseServiceState{ pid: pid, slot: slot, next: 1, ids: primitives.zero_payload(), kinds: primitives.zero_payload(), firsts: primitives.zero_payload(), seconds: primitives.zero_payload(), thirds: primitives.zero_payload(), gens: primitives.zero_payload(), len: 0 }
}

func leasewith(s: LeaseServiceState, next: u8, ids: [LEASE_CAPACITY]u8, kinds: [LEASE_CAPACITY]u8, firsts: [LEASE_CAPACITY]u8, seconds: [LEASE_CAPACITY]u8, thirds: [LEASE_CAPACITY]u8, gens: [LEASE_CAPACITY]u8, len: usize) LeaseServiceState {
    return LeaseServiceState{ pid: s.pid, slot: s.slot, next: next, ids: ids, kinds: kinds, firsts: firsts, seconds: seconds, thirds: thirds, gens: gens, len: len }
}

func lease_reply(status: syscall.SyscallStatus, len: usize, payload: [4]u8) service_effect.Effect {
    return service_effect.effect_reply(status, len, payload)
}

func lease_failure(code: u8) service_effect.Effect {
    payload: [4]u8 = primitives.zero_payload()
    payload[0] = code
    return lease_reply(syscall.SyscallStatus.InvalidArgument, 1, payload)
}

func lease_issue(s: LeaseServiceState, kind: u8, first: u8, second: u8, third: u8, generation: u8) LeaseResult {
    if first == 0 && kind == LEASE_KIND_COMPLETION {
        return LeaseResult{ state: s, effect: lease_failure(LEASE_INVALID), op: LEASE_OP_NONE, first: 0, second: 0, third: 0 }
    }
    if s.len >= LEASE_CAPACITY {
        return LeaseResult{ state: s, effect: lease_reply(syscall.SyscallStatus.Exhausted, 0, primitives.zero_payload()), op: LEASE_OP_NONE, first: 0, second: 0, third: 0 }
    }

    ids := s.ids
    kinds := s.kinds
    firsts := s.firsts
    seconds := s.seconds
    thirds := s.thirds
    gens := s.gens
    ids[s.len] = s.next
    kinds[s.len] = kind
    firsts[s.len] = first
    seconds[s.len] = second
    thirds[s.len] = third
    gens[s.len] = generation

    payload: [4]u8 = primitives.zero_payload()
    payload[0] = s.next
    payload[1] = first
    return LeaseResult{ state: leasewith(s, s.next + 1, ids, kinds, firsts, seconds, thirds, gens, s.len + 1), effect: lease_reply(syscall.SyscallStatus.Ok, 2, payload), op: LEASE_OP_NONE, first: 0, second: 0, third: 0 }
}

func lease_issue_completion(s: LeaseServiceState, workflow: u8, generation: u8) LeaseResult {
    return lease_issue(s, LEASE_KIND_COMPLETION, workflow, 0, 0, generation)
}

func lease_issue_installer_apply(s: LeaseServiceState, generation: u8) LeaseResult {
    return lease_issue(s, LEASE_KIND_INSTALLER_APPLY, 0, 0, 0, generation)
}

func lease_issue_object_update(s: LeaseServiceState, object_store: object_store_service.ObjectStoreServiceState, name: u8, value: u8, generation: u8) LeaseResult {
    version := object_store_service.object_current_version(object_store, name)
    if version == 0 {
        return LeaseResult{ state: s, effect: lease_failure(LEASE_INVALID), op: LEASE_OP_NONE, first: 0, second: 0, third: 0 }
    }
    return lease_issue(s, LEASE_KIND_OBJECT_UPDATE, name, value, version, generation)
}

func lease_issue_external_ticket(s: LeaseServiceState, epoch: u8, id: u8, generation: u8) LeaseResult {
    if epoch == 0 || id == 0 {
        return LeaseResult{ state: s, effect: lease_failure(LEASE_INVALID), op: LEASE_OP_NONE, first: 0, second: 0, third: 0 }
    }
    return lease_issue(s, LEASE_KIND_EXTERNAL_TICKET, epoch, id, 0, generation)
}

func lease_remove_at(s: LeaseServiceState, idx: usize) LeaseServiceState {
    ids: [LEASE_CAPACITY]u8 = primitives.zero_payload()
    kinds: [LEASE_CAPACITY]u8 = primitives.zero_payload()
    firsts: [LEASE_CAPACITY]u8 = primitives.zero_payload()
    seconds: [LEASE_CAPACITY]u8 = primitives.zero_payload()
    thirds: [LEASE_CAPACITY]u8 = primitives.zero_payload()
    gens: [LEASE_CAPACITY]u8 = primitives.zero_payload()
    for keep in 0..idx {
        ids[keep] = s.ids[keep]
        kinds[keep] = s.kinds[keep]
        firsts[keep] = s.firsts[keep]
        seconds[keep] = s.seconds[keep]
        thirds[keep] = s.thirds[keep]
        gens[keep] = s.gens[keep]
    }
    for keep in idx + 1..s.len {
        ids[keep - 1] = s.ids[keep]
        kinds[keep - 1] = s.kinds[keep]
        firsts[keep - 1] = s.firsts[keep]
        seconds[keep - 1] = s.seconds[keep]
        thirds[keep - 1] = s.thirds[keep]
        gens[keep - 1] = s.gens[keep]
    }
    return leasewith(s, s.next, ids, kinds, firsts, seconds, thirds, gens, s.len - 1)
}

func lease_consume_completion(s: LeaseServiceState, id: u8, workflow: u8, generation: u8) LeaseResult {
    for i in 0..s.len {
        if s.ids[i] != id {
            continue
        }
        if s.kinds[i] != LEASE_KIND_COMPLETION || s.firsts[i] != workflow {
            return LeaseResult{ state: s, effect: lease_failure(LEASE_INVALID), op: LEASE_OP_NONE, first: 0, second: 0, third: 0 }
        }

        next := lease_remove_at(s, i)
        if s.gens[i] != generation {
            return LeaseResult{ state: next, effect: lease_failure(LEASE_STALE), op: LEASE_OP_NONE, first: 0, second: 0, third: 0 }
        }
        return LeaseResult{ state: next, effect: lease_reply(syscall.SyscallStatus.Ok, 0, primitives.zero_payload()), op: LEASE_OP_TAKE_COMPLETION, first: workflow, second: 0, third: 0 }
    }
    return LeaseResult{ state: s, effect: lease_failure(LEASE_INVALID), op: LEASE_OP_NONE, first: 0, second: 0, third: 0 }
}

func lease_missing_installer_apply_code(s: LeaseServiceState, id: u8) u8 {
    if id != 0 && id < s.next {
        return LEASE_CONSUMED
    }
    return LEASE_INVALID
}

func lease_consume_installer_apply(s: LeaseServiceState, id: u8, generation: u8) LeaseResult {
    for i in 0..s.len {
        if s.ids[i] != id {
            continue
        }
        if s.kinds[i] != LEASE_KIND_INSTALLER_APPLY {
            return LeaseResult{ state: s, effect: lease_failure(LEASE_INVALID), op: LEASE_OP_NONE, first: 0, second: 0, third: 0 }
        }

        next := lease_remove_at(s, i)
        if s.gens[i] != generation {
            return LeaseResult{ state: next, effect: lease_failure(LEASE_STALE), op: LEASE_OP_NONE, first: 0, second: 0, third: 0 }
        }
        return LeaseResult{ state: next, effect: lease_reply(syscall.SyscallStatus.Ok, 0, primitives.zero_payload()), op: LEASE_OP_USE_INSTALLER_APPLY, first: 0, second: 0, third: 0 }
    }
    return LeaseResult{ state: s, effect: lease_failure(lease_missing_installer_apply_code(s, id)), op: LEASE_OP_NONE, first: 0, second: 0, third: 0 }
}

func lease_consume_object_update(s: LeaseServiceState, id: u8, generation: u8) LeaseResult {
    for i in 0..s.len {
        if s.ids[i] != id {
            continue
        }
        if s.kinds[i] != LEASE_KIND_OBJECT_UPDATE {
            return LeaseResult{ state: s, effect: lease_failure(LEASE_INVALID), op: LEASE_OP_NONE, first: 0, second: 0, third: 0 }
        }

        next := lease_remove_at(s, i)
        if s.gens[i] != generation {
            return LeaseResult{ state: next, effect: lease_failure(LEASE_STALE), op: LEASE_OP_NONE, first: 0, second: 0, third: 0 }
        }
        return LeaseResult{ state: next, effect: lease_reply(syscall.SyscallStatus.Ok, 0, primitives.zero_payload()), op: LEASE_OP_SCHEDULE_OBJECT_UPDATE, first: s.firsts[i], second: s.seconds[i], third: s.thirds[i] }
    }
    return LeaseResult{ state: s, effect: lease_failure(LEASE_INVALID), op: LEASE_OP_NONE, first: 0, second: 0, third: 0 }
}

func lease_consume_external_ticket(s: LeaseServiceState, id: u8, generation: u8) LeaseResult {
    for i in 0..s.len {
        if s.ids[i] != id {
            continue
        }
        if s.kinds[i] != LEASE_KIND_EXTERNAL_TICKET {
            return LeaseResult{ state: s, effect: lease_failure(LEASE_INVALID), op: LEASE_OP_NONE, first: 0, second: 0, third: 0 }
        }

        next := lease_remove_at(s, i)
        if s.gens[i] != generation {
            return LeaseResult{ state: next, effect: lease_failure(LEASE_STALE), op: LEASE_OP_NONE, first: 0, second: 0, third: 0 }
        }
        return LeaseResult{ state: next, effect: lease_reply(syscall.SyscallStatus.Ok, 0, primitives.zero_payload()), op: LEASE_OP_USE_EXTERNAL_TICKET, first: s.firsts[i], second: s.seconds[i], third: 0 }
    }
    return LeaseResult{ state: s, effect: lease_failure(LEASE_INVALID), op: LEASE_OP_NONE, first: 0, second: 0, third: 0 }
}

func handle(s: LeaseServiceState, object_store: object_store_service.ObjectStoreServiceState, m: service_effect.Message, completion_generation: u8, workset_generation: u8, ticket_generation: u8) LeaseResult {
    if m.payload_len == 0 {
        return LeaseResult{ state: s, effect: lease_failure(LEASE_INVALID), op: LEASE_OP_NONE, first: 0, second: 0, third: 0 }
    }

    switch m.payload[0] {
    case LEASE_CMD_ISSUE_COMPLETION:
        if m.payload_len != 2 {
            return LeaseResult{ state: s, effect: lease_failure(LEASE_INVALID), op: LEASE_OP_NONE, first: 0, second: 0, third: 0 }
        }
        return lease_issue_completion(s, m.payload[1], completion_generation)
    case LEASE_CMD_ISSUE_INSTALLER_APPLY:
        if m.payload_len != 1 {
            return LeaseResult{ state: s, effect: lease_failure(LEASE_INVALID), op: LEASE_OP_NONE, first: 0, second: 0, third: 0 }
        }
        return lease_issue_installer_apply(s, workset_generation)
    case LEASE_CMD_CONSUME_COMPLETION:
        if m.payload_len != 3 {
            return LeaseResult{ state: s, effect: lease_failure(LEASE_INVALID), op: LEASE_OP_NONE, first: 0, second: 0, third: 0 }
        }
        return lease_consume_completion(s, m.payload[1], m.payload[2], completion_generation)
    case LEASE_CMD_ISSUE_OBJECT_UPDATE:
        if m.payload_len != 3 {
            return LeaseResult{ state: s, effect: lease_failure(LEASE_INVALID), op: LEASE_OP_NONE, first: 0, second: 0, third: 0 }
        }
        return lease_issue_object_update(s, object_store, m.payload[1], m.payload[2], workset_generation)
    case LEASE_CMD_ISSUE_EXTERNAL_TICKET:
        if m.payload_len != 3 {
            return LeaseResult{ state: s, effect: lease_failure(LEASE_INVALID), op: LEASE_OP_NONE, first: 0, second: 0, third: 0 }
        }
        return lease_issue_external_ticket(s, m.payload[1], m.payload[2], ticket_generation)
    case LEASE_CMD_CONSUME_OBJECT_UPDATE:
        if m.payload_len != 2 {
            return LeaseResult{ state: s, effect: lease_failure(LEASE_INVALID), op: LEASE_OP_NONE, first: 0, second: 0, third: 0 }
        }
        return lease_consume_object_update(s, m.payload[1], workset_generation)
    default:
        return LeaseResult{ state: s, effect: lease_failure(LEASE_INVALID), op: LEASE_OP_NONE, first: 0, second: 0, third: 0 }
    }
}

func lease_count(s: LeaseServiceState) usize {
    return s.len
}