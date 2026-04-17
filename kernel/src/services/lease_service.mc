import primitives
import service_effect
import syscall

const LEASE_CAPACITY: usize = 4

const LEASE_OP_NONE: u8 = 0
const LEASE_OP_TAKE: u8 = 1

const LEASE_STALE: u8 = 83    // 'S'
const LEASE_INVALID: u8 = 73  // 'I'

struct LeaseServiceState {
    pid: u32
    slot: u32
    next: u8
    ids: [LEASE_CAPACITY]u8
    workflows: [LEASE_CAPACITY]u8
    gens: [LEASE_CAPACITY]u8
    len: usize
}

struct LeaseResult {
    state: LeaseServiceState
    effect: service_effect.Effect
    op: u8
    workflow: u8
}

func lease_init(pid: u32, slot: u32) LeaseServiceState {
    return LeaseServiceState{ pid: pid, slot: slot, next: 1, ids: primitives.zero_payload(), workflows: primitives.zero_payload(), gens: primitives.zero_payload(), len: 0 }
}

func leasewith(s: LeaseServiceState, next: u8, ids: [LEASE_CAPACITY]u8, workflows: [LEASE_CAPACITY]u8, gens: [LEASE_CAPACITY]u8, len: usize) LeaseServiceState {
    return LeaseServiceState{ pid: s.pid, slot: s.slot, next: next, ids: ids, workflows: workflows, gens: gens, len: len }
}

func lease_reply(status: syscall.SyscallStatus, len: usize, payload: [4]u8) service_effect.Effect {
    return service_effect.effect_reply(status, len, payload)
}

func lease_failure(code: u8) service_effect.Effect {
    payload: [4]u8 = primitives.zero_payload()
    payload[0] = code
    return lease_reply(syscall.SyscallStatus.InvalidArgument, 1, payload)
}

func lease_issue(s: LeaseServiceState, workflow: u8, generation: u8) LeaseResult {
    if workflow == 0 {
        return LeaseResult{ state: s, effect: lease_failure(LEASE_INVALID), op: LEASE_OP_NONE, workflow: 0 }
    }
    if s.len >= LEASE_CAPACITY {
        return LeaseResult{ state: s, effect: lease_reply(syscall.SyscallStatus.Exhausted, 0, primitives.zero_payload()), op: LEASE_OP_NONE, workflow: 0 }
    }

    ids := s.ids
    workflows := s.workflows
    gens := s.gens
    ids[s.len] = s.next
    workflows[s.len] = workflow
    gens[s.len] = generation

    payload: [4]u8 = primitives.zero_payload()
    payload[0] = s.next
    payload[1] = workflow
    return LeaseResult{ state: leasewith(s, s.next + 1, ids, workflows, gens, s.len + 1), effect: lease_reply(syscall.SyscallStatus.Ok, 2, payload), op: LEASE_OP_NONE, workflow: 0 }
}

func lease_remove_at(s: LeaseServiceState, idx: usize) LeaseServiceState {
    ids: [LEASE_CAPACITY]u8 = primitives.zero_payload()
    workflows: [LEASE_CAPACITY]u8 = primitives.zero_payload()
    gens: [LEASE_CAPACITY]u8 = primitives.zero_payload()
    for keep in 0..idx {
        ids[keep] = s.ids[keep]
        workflows[keep] = s.workflows[keep]
        gens[keep] = s.gens[keep]
    }
    for keep in idx + 1..s.len {
        ids[keep - 1] = s.ids[keep]
        workflows[keep - 1] = s.workflows[keep]
        gens[keep - 1] = s.gens[keep]
    }
    return leasewith(s, s.next, ids, workflows, gens, s.len - 1)
}

func lease_consume(s: LeaseServiceState, id: u8, workflow: u8, generation: u8) LeaseResult {
    for i in 0..s.len {
        if s.ids[i] != id {
            continue
        }
        if s.workflows[i] != workflow {
            return LeaseResult{ state: s, effect: lease_failure(LEASE_INVALID), op: LEASE_OP_NONE, workflow: 0 }
        }

        next := lease_remove_at(s, i)
        if s.gens[i] != generation {
            return LeaseResult{ state: next, effect: lease_failure(LEASE_STALE), op: LEASE_OP_NONE, workflow: 0 }
        }
        return LeaseResult{ state: next, effect: lease_reply(syscall.SyscallStatus.Ok, 0, primitives.zero_payload()), op: LEASE_OP_TAKE, workflow: workflow }
    }
    return LeaseResult{ state: s, effect: lease_failure(LEASE_INVALID), op: LEASE_OP_NONE, workflow: 0 }
}

func handle(s: LeaseServiceState, m: service_effect.Message, generation: u8) LeaseResult {
    if m.payload_len == 2 {
        return lease_issue(s, m.payload[1], generation)
    }
    if m.payload_len == 3 {
        return lease_consume(s, m.payload[1], m.payload[2], generation)
    }
    return LeaseResult{ state: s, effect: lease_failure(LEASE_INVALID), op: LEASE_OP_NONE, workflow: 0 }
}

func lease_count(s: LeaseServiceState) usize {
    return s.len
}