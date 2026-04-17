import journal_service
import completion_mailbox_service
import primitives
import service_effect
import syscall
import task_service
import timer_service

const WORKFLOW_OP_SCHEDULE: u8 = 83  // 'S'
const WORKFLOW_OP_QUERY: u8 = 81     // 'Q'
const WORKFLOW_OP_CANCEL: u8 = 67    // 'C'

const WORKFLOW_STATE_IDLE: u8 = 73       // 'I'
const WORKFLOW_STATE_WAITING: u8 = 87    // 'W'
const WORKFLOW_STATE_RUNNING: u8 = 82    // 'R'
const WORKFLOW_STATE_DELIVERING: u8 = 77 // 'M'
const WORKFLOW_STATE_DONE: u8 = 68       // 'D'
const WORKFLOW_STATE_CANCELLED: u8 = 67  // 'C'
const WORKFLOW_STATE_FAILED: u8 = 70     // 'F'

const WORKFLOW_RESTART_NONE: u8 = 78       // 'N'
const WORKFLOW_RESTART_RESUMED: u8 = 82    // 'R'
const WORKFLOW_RESTART_CANCELLED: u8 = 67  // 'C'

const WORKFLOW_TIMER_ID: u8 = 1
const WORKFLOW_DELIVERY_WOULDBLOCK: u8 = 2
const WORKFLOW_DELIVERY_EXHAUSTED: u8 = 7

struct WorkflowServiceState {
    pid: u32
    id: u8
    due: u8
    opcode: u8
    task: u8
    state: u8
    restart: u8
    generation: u8
}

struct WorkflowSnapshot {
    id: u8
    due: u8
    opcode: u8
    task: u8
    state: u8
    restart: u8
    generation: u8
}

struct WorkflowResult {
    state: WorkflowServiceState
    effect: service_effect.Effect
}

struct WorkflowAdvanceResult {
    workflow: WorkflowServiceState
    timer: timer_service.TimerServiceState
    task: task_service.TaskServiceState
    completion: completion_mailbox_service.CompletionMailboxServiceState
}

struct WorkflowStepResult {
    workflow: WorkflowServiceState
    timer: timer_service.TimerServiceState
    task: task_service.TaskServiceState
    journal: journal_service.JournalServiceState
    completion: completion_mailbox_service.CompletionMailboxServiceState
    effect: service_effect.Effect
}

struct WorkflowDeliveryResult {
    workflow: WorkflowServiceState
    completion: completion_mailbox_service.CompletionMailboxServiceState
}

func workflow_init(pid: u32) WorkflowServiceState {
    return WorkflowServiceState{ pid: pid, id: 0, due: 0, opcode: 0, task: 0, state: WORKFLOW_STATE_IDLE, restart: WORKFLOW_RESTART_NONE, generation: 0 }
}

func workflowwith(s: WorkflowServiceState, id: u8, due: u8, opcode: u8, task: u8, state: u8, restart: u8, generation: u8) WorkflowServiceState {
    return WorkflowServiceState{ pid: s.pid, id: id, due: due, opcode: opcode, task: task, state: state, restart: restart, generation: generation }
}

func workflow_snapshot(s: WorkflowServiceState) WorkflowSnapshot {
    return WorkflowSnapshot{ id: s.id, due: s.due, opcode: s.opcode, task: s.task, state: s.state, restart: s.restart, generation: s.generation }
}

func workflow_reload(pid: u32, snap: WorkflowSnapshot) WorkflowServiceState {
    return WorkflowServiceState{ pid: pid, id: snap.id, due: snap.due, opcode: snap.opcode, task: snap.task, state: snap.state, restart: snap.restart, generation: snap.generation }
}

func workflow_reply_invalid() service_effect.Effect {
    return service_effect.effect_reply(syscall.SyscallStatus.InvalidArgument, 0, primitives.zero_payload())
}

func workflow_schedule_effect(id: u8, state: u8) service_effect.Effect {
    payload: [4]u8 = primitives.zero_payload()
    payload[0] = id
    payload[1] = state
    return service_effect.effect_reply(syscall.SyscallStatus.Ok, 2, payload)
}

func workflow_query_effect(s: WorkflowServiceState) service_effect.Effect {
    payload: [4]u8 = primitives.zero_payload()
    payload[0] = s.state
    payload[1] = s.restart
    payload[2] = s.task
    payload[3] = s.generation
    return service_effect.effect_reply(workflow_query_status(s), 4, payload)
}

func workflow_matches(s: WorkflowServiceState, id: u8) bool {
    if s.state == WORKFLOW_STATE_IDLE {
        return false
    }
    return s.id == id
}

func workflow_is_active(s: WorkflowServiceState) bool {
    return s.state == WORKFLOW_STATE_WAITING || s.state == WORKFLOW_STATE_RUNNING || s.state == WORKFLOW_STATE_DELIVERING
}

func workflow_has_record(s: WorkflowServiceState) bool {
    return s.state != WORKFLOW_STATE_IDLE
}

func workflow_prepare_schedule(s: WorkflowServiceState, generation: u8) WorkflowServiceState {
    if workflow_is_active(s) {
        return s
    }
    return workflowwith(s, s.id, s.due, s.opcode, s.task, s.state, WORKFLOW_RESTART_NONE, generation)
}

func workflow_schedule(s: WorkflowServiceState, id: u8, due: u8) WorkflowResult {
    if id == 0 {
        return WorkflowResult{ state: s, effect: workflow_reply_invalid() }
    }
    if workflow_is_active(s) {
        return WorkflowResult{ state: s, effect: workflow_reply_invalid() }
    }
    next := workflowwith(s, id, due, id, 0, WORKFLOW_STATE_WAITING, WORKFLOW_RESTART_NONE, s.generation)
    return WorkflowResult{ state: next, effect: workflow_schedule_effect(id, WORKFLOW_STATE_WAITING) }
}

func workflow_cancel(s: WorkflowServiceState, id: u8) WorkflowResult {
    if !workflow_matches(s, id) || !workflow_is_active(s) {
        return WorkflowResult{ state: s, effect: workflow_reply_invalid() }
    }
    next := workflowwith(s, s.id, 0, s.opcode, 0, WORKFLOW_STATE_CANCELLED, s.restart, s.generation)
    return WorkflowResult{ state: next, effect: service_effect.effect_reply(syscall.SyscallStatus.Ok, 0, primitives.zero_payload()) }
}

func workflow_query(s: WorkflowServiceState, id: u8) WorkflowResult {
    if !workflow_matches(s, id) {
        return WorkflowResult{ state: s, effect: workflow_reply_invalid() }
    }
    return WorkflowResult{ state: s, effect: workflow_query_effect(s) }
}

func handle(s: WorkflowServiceState, m: service_effect.Message) WorkflowResult {
    if m.payload_len == 0 {
        return WorkflowResult{ state: s, effect: workflow_reply_invalid() }
    }

    switch m.payload[0] {
    case WORKFLOW_OP_SCHEDULE:
        if m.payload_len != 3 {
            return WorkflowResult{ state: s, effect: workflow_reply_invalid() }
        }
        return workflow_schedule(s, m.payload[1], m.payload[2])
    case WORKFLOW_OP_QUERY:
        if m.payload_len != 2 {
            return WorkflowResult{ state: s, effect: workflow_reply_invalid() }
        }
        return workflow_query(s, m.payload[1])
    case WORKFLOW_OP_CANCEL:
        if m.payload_len != 2 {
            return WorkflowResult{ state: s, effect: workflow_reply_invalid() }
        }
        return workflow_cancel(s, m.payload[1])
    default:
        return WorkflowResult{ state: s, effect: workflow_reply_invalid() }
    }
}

func workflow_waiting(s: WorkflowServiceState, due: u8, restart: u8, generation: u8) WorkflowServiceState {
    return workflowwith(s, s.id, due, s.opcode, 0, WORKFLOW_STATE_WAITING, restart, generation)
}

func workflow_running(s: WorkflowServiceState, task: u8) WorkflowServiceState {
    return workflowwith(s, s.id, 0, s.opcode, task, WORKFLOW_STATE_RUNNING, s.restart, s.generation)
}

func workflow_pending_delivery(s: WorkflowServiceState, outcome: u8, status: u8) WorkflowServiceState {
    return workflowwith(s, s.id, status, s.opcode, outcome, WORKFLOW_STATE_DELIVERING, s.restart, s.generation)
}

func workflow_terminal(s: WorkflowServiceState, state: u8) WorkflowServiceState {
    return workflowwith(s, s.id, 0, s.opcode, 0, state, s.restart, s.generation)
}

func workflow_delivery_status_code(status: syscall.SyscallStatus) u8 {
    if status == syscall.SyscallStatus.WouldBlock {
        return WORKFLOW_DELIVERY_WOULDBLOCK
    }
    if status == syscall.SyscallStatus.Exhausted {
        return WORKFLOW_DELIVERY_EXHAUSTED
    }
    return 0
}

func workflow_query_status(s: WorkflowServiceState) syscall.SyscallStatus {
    if s.state != WORKFLOW_STATE_DELIVERING {
        return syscall.SyscallStatus.Ok
    }
    if s.due == WORKFLOW_DELIVERY_WOULDBLOCK {
        return syscall.SyscallStatus.WouldBlock
    }
    if s.due == WORKFLOW_DELIVERY_EXHAUSTED {
        return syscall.SyscallStatus.Exhausted
    }
    return syscall.SyscallStatus.InvalidArgument
}

func workflow_deliver_completion(s: WorkflowServiceState, completion: completion_mailbox_service.CompletionMailboxServiceState, outcome: u8) WorkflowDeliveryResult {
    queued := completion_mailbox_service.completion_mailbox_enqueue(completion, s.id, outcome, s.restart, s.generation)
    if service_effect.effect_reply_status(queued.effect) == syscall.SyscallStatus.Ok {
        return WorkflowDeliveryResult{ workflow: workflow_terminal(s, outcome), completion: queued.state }
    }
    status := workflow_delivery_status_code(service_effect.effect_reply_status(queued.effect))
    return WorkflowDeliveryResult{ workflow: workflow_pending_delivery(s, outcome, status), completion: queued.state }
}

func workflow_advance(s: WorkflowServiceState, timer: timer_service.TimerServiceState, task: task_service.TaskServiceState, completion: completion_mailbox_service.CompletionMailboxServiceState) WorkflowAdvanceResult {
    next_timer: timer_service.TimerServiceState = timer
    next_task: task_service.TaskServiceState = task
    next_workflow: WorkflowServiceState = s
    next_completion: completion_mailbox_service.CompletionMailboxServiceState = completion

    if s.state == WORKFLOW_STATE_WAITING {
        idx := timer_service.timer_find(timer, WORKFLOW_TIMER_ID)
        if idx >= timer_service.TIMER_CAPACITY {
            missing_delivery := workflow_deliver_completion(s, next_completion, WORKFLOW_STATE_CANCELLED)
            return WorkflowAdvanceResult{ workflow: missing_delivery.workflow, timer: next_timer, task: next_task, completion: missing_delivery.completion }
        }

        due := timer.due[idx]
        if timer.state[idx] == timer_service.TIMER_STATE_ACTIVE {
            next_workflow = workflow_waiting(s, due, s.restart, s.generation)
            return WorkflowAdvanceResult{ workflow: next_workflow, timer: next_timer, task: next_task, completion: next_completion }
        }
        if timer.state[idx] == timer_service.TIMER_STATE_CANCELLED {
            cancelled_delivery := workflow_deliver_completion(s, next_completion, WORKFLOW_STATE_CANCELLED)
            return WorkflowAdvanceResult{ workflow: cancelled_delivery.workflow, timer: next_timer, task: next_task, completion: cancelled_delivery.completion }
        }
        if timer.state[idx] != timer_service.TIMER_STATE_EXPIRED {
            invalid_timer_delivery := workflow_deliver_completion(s, next_completion, WORKFLOW_STATE_FAILED)
            return WorkflowAdvanceResult{ workflow: invalid_timer_delivery.workflow, timer: next_timer, task: next_task, completion: invalid_timer_delivery.completion }
        }

        submit := task_service.task_submit(task, s.opcode)
        next_task = submit.state
        if service_effect.effect_reply_status(submit.effect) != syscall.SyscallStatus.Ok {
            submit_failure_delivery := workflow_deliver_completion(s, next_completion, WORKFLOW_STATE_FAILED)
            return WorkflowAdvanceResult{ workflow: submit_failure_delivery.workflow, timer: next_timer, task: next_task, completion: submit_failure_delivery.completion }
        }

        payload := service_effect.effect_reply_payload(submit.effect)
        if payload[1] == task_service.TASK_STATE_FAILED {
            immediate_failure_delivery := workflow_deliver_completion(s, next_completion, WORKFLOW_STATE_FAILED)
            return WorkflowAdvanceResult{ workflow: immediate_failure_delivery.workflow, timer: next_timer, task: next_task, completion: immediate_failure_delivery.completion }
        }

        next_workflow = workflow_running(s, payload[0])
        return WorkflowAdvanceResult{ workflow: next_workflow, timer: next_timer, task: next_task, completion: next_completion }
    }

    if s.state == WORKFLOW_STATE_RUNNING {
        complete := task_service.task_complete(task, s.task)
        next_task = complete.state
        if service_effect.effect_reply_status(complete.effect) == syscall.SyscallStatus.Ok {
            done_delivery := workflow_deliver_completion(s, next_completion, WORKFLOW_STATE_DONE)
            return WorkflowAdvanceResult{ workflow: done_delivery.workflow, timer: next_timer, task: next_task, completion: done_delivery.completion }
        }

        query := task_service.task_query(next_task, s.task)
        next_task = query.state
        if service_effect.effect_reply_status(query.effect) != syscall.SyscallStatus.Ok {
            query_failure_delivery := workflow_deliver_completion(s, next_completion, WORKFLOW_STATE_FAILED)
            return WorkflowAdvanceResult{ workflow: query_failure_delivery.workflow, timer: next_timer, task: next_task, completion: query_failure_delivery.completion }
        }

        query_payload := service_effect.effect_reply_payload(query.effect)
        if query_payload[0] == task_service.TASK_STATE_CANCELLED {
            task_cancelled_delivery := workflow_deliver_completion(s, next_completion, WORKFLOW_STATE_CANCELLED)
            return WorkflowAdvanceResult{ workflow: task_cancelled_delivery.workflow, timer: next_timer, task: next_task, completion: task_cancelled_delivery.completion }
        }
        if query_payload[0] == task_service.TASK_STATE_DONE {
            query_done_delivery := workflow_deliver_completion(s, next_completion, WORKFLOW_STATE_DONE)
            return WorkflowAdvanceResult{ workflow: query_done_delivery.workflow, timer: next_timer, task: next_task, completion: query_done_delivery.completion }
        }
        if query_payload[0] == task_service.TASK_STATE_FAILED {
            task_failed_delivery := workflow_deliver_completion(s, next_completion, WORKFLOW_STATE_FAILED)
            return WorkflowAdvanceResult{ workflow: task_failed_delivery.workflow, timer: next_timer, task: next_task, completion: task_failed_delivery.completion }
        }
    }

    if s.state == WORKFLOW_STATE_DELIVERING {
        retry_delivery := workflow_deliver_completion(s, next_completion, s.task)
        return WorkflowAdvanceResult{ workflow: retry_delivery.workflow, timer: next_timer, task: next_task, completion: retry_delivery.completion }
    }

    return WorkflowAdvanceResult{ workflow: next_workflow, timer: next_timer, task: next_task, completion: next_completion }
}

func workflow_lane_bytes(s: WorkflowServiceState) [4]u8 {
    data: [4]u8 = primitives.zero_payload()
    data[0] = s.state
    data[1] = s.id
    data[2] = s.due
    data[3] = s.opcode
    return data
}

func workflow_sync_journal(journal: journal_service.JournalServiceState, s: WorkflowServiceState) journal_service.JournalServiceState {
    lane := journal_service.lane_init(journal_service.JOURNAL_LANE_B)
    if workflow_has_record(s) || s.restart != WORKFLOW_RESTART_NONE {
        lane = journal_service.JournalLane{ name: journal_service.JOURNAL_LANE_B, len: 4, data: workflow_lane_bytes(s) }
    }
    return journal_service.journalwith(journal, journal.lane0, lane)
}

func workflow_lane_matches(s: WorkflowServiceState, lane: journal_service.JournalLane) bool {
    if lane.name != journal_service.JOURNAL_LANE_B || lane.len != 4 {
        return false
    }
    bytes := workflow_lane_bytes(s)
    for i in 0..4 {
        if lane.data[i] != bytes[i] {
            return false
        }
    }
    return true
}

func workflow_restart_reload(pid: u32, snap: WorkflowSnapshot, lane: journal_service.JournalLane, generation: u8, keep: bool) WorkflowServiceState {
    current := workflow_reload(pid, snap)
    if !workflow_has_record(current) {
        return current
    }
    if current.state == WORKFLOW_STATE_DELIVERING {
        return workflowwith(current, current.id, current.due, current.opcode, current.task, current.state, WORKFLOW_RESTART_NONE, current.generation)
    }
    if !workflow_is_active(current) {
        return workflowwith(current, current.id, current.due, current.opcode, current.task, current.state, WORKFLOW_RESTART_NONE, current.generation)
    }
    if !keep || current.generation != generation || !workflow_lane_matches(current, lane) {
        return workflowwith(current, current.id, 0, current.opcode, 0, WORKFLOW_STATE_CANCELLED, WORKFLOW_RESTART_CANCELLED, generation)
    }
    if current.state == WORKFLOW_STATE_RUNNING {
        return workflowwith(current, current.id, 0, current.opcode, 0, WORKFLOW_STATE_WAITING, WORKFLOW_RESTART_RESUMED, generation)
    }
    return workflowwith(current, current.id, current.due, current.opcode, 0, WORKFLOW_STATE_WAITING, WORKFLOW_RESTART_RESUMED, generation)
}

func step(s: WorkflowServiceState, timer: timer_service.TimerServiceState, task: task_service.TaskServiceState, journal: journal_service.JournalServiceState, completion: completion_mailbox_service.CompletionMailboxServiceState, msg: service_effect.Message, generation: u8) WorkflowStepResult {
    ticked_timer := timer_service.timer_tick(timer)
    advanced := workflow_advance(s, ticked_timer, task, completion)
    next_workflow := advanced.workflow
    next_timer := advanced.timer
    next_task := advanced.task
    next_completion := advanced.completion
    next_journal := journal

    if msg.payload_len == 0 {
        next_journal = workflow_sync_journal(journal, next_workflow)
        return WorkflowStepResult{ workflow: next_workflow, timer: next_timer, task: next_task, journal: next_journal, completion: next_completion, effect: workflow_reply_invalid() }
    }

    if msg.payload[0] == WORKFLOW_OP_SCHEDULE {
        prepared := workflow_prepare_schedule(next_workflow, generation)
        scheduled := handle(prepared, msg)
        if service_effect.effect_reply_status(scheduled.effect) != syscall.SyscallStatus.Ok {
            next_journal = workflow_sync_journal(journal, next_workflow)
            return WorkflowStepResult{ workflow: next_workflow, timer: next_timer, task: next_task, journal: next_journal, completion: next_completion, effect: scheduled.effect }
        }
        create := timer_service.timer_create(next_timer, WORKFLOW_TIMER_ID, msg.payload[2])
        next_timer = create.state
        if service_effect.effect_reply_status(create.effect) != syscall.SyscallStatus.Ok {
            next_journal = workflow_sync_journal(journal, next_workflow)
            return WorkflowStepResult{ workflow: next_workflow, timer: next_timer, task: next_task, journal: next_journal, completion: next_completion, effect: create.effect }
        }
        next_workflow = scheduled.state
        next_journal = workflow_sync_journal(journal, next_workflow)
        return WorkflowStepResult{ workflow: next_workflow, timer: next_timer, task: next_task, journal: next_journal, completion: next_completion, effect: scheduled.effect }
    }

    if msg.payload[0] == WORKFLOW_OP_CANCEL {
        if msg.payload_len != 2 || !workflow_matches(next_workflow, msg.payload[1]) || !workflow_is_active(next_workflow) {
            next_journal = workflow_sync_journal(journal, next_workflow)
            return WorkflowStepResult{ workflow: next_workflow, timer: next_timer, task: next_task, journal: next_journal, completion: next_completion, effect: workflow_reply_invalid() }
        }

        if next_workflow.state == WORKFLOW_STATE_WAITING {
            cancel_timer := timer_service.timer_cancel(next_timer, WORKFLOW_TIMER_ID)
            next_timer = cancel_timer.state
            if service_effect.effect_reply_status(cancel_timer.effect) != syscall.SyscallStatus.Ok {
                next_journal = workflow_sync_journal(journal, next_workflow)
                return WorkflowStepResult{ workflow: next_workflow, timer: next_timer, task: next_task, journal: next_journal, completion: next_completion, effect: cancel_timer.effect }
            }
        } else {
            cancel_task := task_service.task_cancel(next_task, next_workflow.task)
            next_task = cancel_task.state
            if service_effect.effect_reply_status(cancel_task.effect) != syscall.SyscallStatus.Ok {
                next_journal = workflow_sync_journal(journal, next_workflow)
                return WorkflowStepResult{ workflow: next_workflow, timer: next_timer, task: next_task, journal: next_journal, completion: next_completion, effect: cancel_task.effect }
            }
        }

        cancelled := handle(next_workflow, msg)
        next_workflow = cancelled.state
        next_journal = workflow_sync_journal(journal, next_workflow)
        return WorkflowStepResult{ workflow: next_workflow, timer: next_timer, task: next_task, journal: next_journal, completion: next_completion, effect: cancelled.effect }
    }

    handled := handle(next_workflow, msg)
    next_workflow = handled.state
    next_journal = workflow_sync_journal(journal, next_workflow)
    return WorkflowStepResult{ workflow: next_workflow, timer: next_timer, task: next_task, journal: next_journal, completion: next_completion, effect: handled.effect }
}

func workflow_active_count(s: WorkflowServiceState) usize {
    if workflow_is_active(s) {
        return 1
    }
    return 0
}