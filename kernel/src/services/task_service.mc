import primitives
import service_effect
import syscall

const TASK_CAPACITY: usize = 4

const TASK_OP_SUBMIT: u8 = 83  // 'S'
const TASK_OP_QUERY: u8 = 81   // 'Q'
const TASK_OP_CANCEL: u8 = 67  // 'C'
const TASK_OP_LIST: u8 = 76    // 'L'

const TASK_STATE_NONE: u8 = 78       // 'N'
const TASK_STATE_ACTIVE: u8 = 65     // 'A'
const TASK_STATE_DONE: u8 = 68       // 'D'
const TASK_STATE_CANCELLED: u8 = 67  // 'C'
const TASK_STATE_FAILED: u8 = 70     // 'F'

struct TaskServiceState {
    pid: u32
    slot: u32
    ids: [TASK_CAPACITY]u8
    op: [TASK_CAPACITY]u8
    state: [TASK_CAPACITY]u8
    len: usize
    next: u8
}

struct TaskResult {
    state: TaskServiceState
    effect: service_effect.Effect
}

func task_init(pid: u32, slot: u32) TaskServiceState {
    return TaskServiceState{ pid: pid, slot: slot, ids: primitives.zero_payload(), op: primitives.zero_payload(), state: primitives.zero_payload(), len: 0, next: 1 }
}

func taskwith(s: TaskServiceState, ids: [TASK_CAPACITY]u8, op: [TASK_CAPACITY]u8, state: [TASK_CAPACITY]u8, len: usize, next: u8) TaskServiceState {
    return TaskServiceState{ pid: s.pid, slot: s.slot, ids: ids, op: op, state: state, len: len, next: next }
}

func task_find(s: TaskServiceState, id: u8) usize {
    for i in 0..s.len {
        if s.ids[i] == id {
            return i
        }
    }
    return TASK_CAPACITY
}

func task_find_free(s: TaskServiceState) usize {
    for i in 0..s.len {
        if s.state[i] == TASK_STATE_DONE || s.state[i] == TASK_STATE_CANCELLED || s.state[i] == TASK_STATE_FAILED {
            return i
        }
    }
    if s.len < TASK_CAPACITY {
        return s.len
    }
    return TASK_CAPACITY
}

func task_reply_status(status: syscall.SyscallStatus, code: u8) service_effect.Effect {
    payload: [4]u8 = primitives.zero_payload()
    if code != 0 {
        payload[0] = code
        return service_effect.effect_reply(status, 1, payload)
    }
    return service_effect.effect_reply(status, 0, payload)
}

func task_submit(s: TaskServiceState, opcode: u8) TaskResult {
    if opcode == 0 {
        return TaskResult{ state: s, effect: task_reply_status(syscall.SyscallStatus.InvalidArgument, 0) }
    }

    slot := task_find_free(s)
    if slot >= TASK_CAPACITY {
        return TaskResult{ state: s, effect: task_reply_status(syscall.SyscallStatus.Exhausted, 0) }
    }

    next_ids: [TASK_CAPACITY]u8 = s.ids
    next_op: [TASK_CAPACITY]u8 = s.op
    next_state: [TASK_CAPACITY]u8 = s.state

    id: u8 = s.next
    next_ids[slot] = id
    next_op[slot] = opcode
    next_state[slot] = TASK_STATE_ACTIVE
    if opcode == 255 {
        next_state[slot] = TASK_STATE_FAILED
    }

    next_len := s.len
    if slot == s.len {
        next_len = s.len + 1
    }

    payload: [4]u8 = primitives.zero_payload()
    payload[0] = id
    payload[1] = next_state[slot]
    return TaskResult{ state: taskwith(s, next_ids, next_op, next_state, next_len, s.next + 1), effect: service_effect.effect_reply(syscall.SyscallStatus.Ok, 2, payload) }
}

func task_query(s: TaskServiceState, id: u8) TaskResult {
    idx := task_find(s, id)
    if idx >= TASK_CAPACITY {
        return TaskResult{ state: s, effect: task_reply_status(syscall.SyscallStatus.InvalidArgument, 0) }
    }
    payload: [4]u8 = primitives.zero_payload()
    payload[0] = s.state[idx]
    payload[1] = s.op[idx]
    return TaskResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.Ok, 2, payload) }
}

func task_cancel(s: TaskServiceState, id: u8) TaskResult {
    idx := task_find(s, id)
    if idx >= TASK_CAPACITY {
        return TaskResult{ state: s, effect: task_reply_status(syscall.SyscallStatus.InvalidArgument, 0) }
    }
    if s.state[idx] != TASK_STATE_ACTIVE {
        return TaskResult{ state: s, effect: task_reply_status(syscall.SyscallStatus.InvalidArgument, s.state[idx]) }
    }
    next_state: [TASK_CAPACITY]u8 = s.state
    next_state[idx] = TASK_STATE_CANCELLED
    return TaskResult{ state: taskwith(s, s.ids, s.op, next_state, s.len, s.next), effect: task_reply_status(syscall.SyscallStatus.Ok, 0) }
}

func task_list_active(s: TaskServiceState, window: u8) TaskResult {
    payload: [4]u8 = primitives.zero_payload()
    limit: usize = usize(window)
    if limit > TASK_CAPACITY {
        limit = TASK_CAPACITY
    }

    count: usize = 0
    for i in 0..s.len {
        if s.state[i] != TASK_STATE_ACTIVE {
            continue
        }
        if count >= limit {
            break
        }
        payload[count] = s.ids[i]
        count = count + 1
    }

    return TaskResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.Ok, count, payload) }
}

func handle(s: TaskServiceState, m: service_effect.Message) TaskResult {
    if m.payload_len == 0 {
        return TaskResult{ state: s, effect: task_reply_status(syscall.SyscallStatus.InvalidArgument, 0) }
    }

    op: u8 = m.payload[0]
    switch op {
    case TASK_OP_SUBMIT:
        if m.payload_len != 2 {
            return TaskResult{ state: s, effect: task_reply_status(syscall.SyscallStatus.InvalidArgument, 0) }
        }
        return task_submit(s, m.payload[1])
    case TASK_OP_QUERY:
        if m.payload_len != 2 {
            return TaskResult{ state: s, effect: task_reply_status(syscall.SyscallStatus.InvalidArgument, 0) }
        }
        return task_query(s, m.payload[1])
    case TASK_OP_CANCEL:
        if m.payload_len != 2 {
            return TaskResult{ state: s, effect: task_reply_status(syscall.SyscallStatus.InvalidArgument, 0) }
        }
        return task_cancel(s, m.payload[1])
    case TASK_OP_LIST:
        if m.payload_len != 2 {
            return TaskResult{ state: s, effect: task_reply_status(syscall.SyscallStatus.InvalidArgument, 0) }
        }
        return task_list_active(s, m.payload[1])
    default:
        return TaskResult{ state: s, effect: task_reply_status(syscall.SyscallStatus.InvalidArgument, 0) }
    }
}

func task_active_count(s: TaskServiceState) usize {
    count: usize = 0
    for i in 0..s.len {
        if s.state[i] == TASK_STATE_ACTIVE {
            count = count + 1
        }
    }
    return count
}
