import journal_service
import completion_mailbox_service
import connection_service
import object_store_service
import primitives
import service_effect
import syscall
import task_service
import timer_service

const WORKFLOW_OP_SCHEDULE: u8 = 83  // 'S'
const WORKFLOW_OP_QUERY: u8 = 81     // 'Q'
const WORKFLOW_OP_CANCEL: u8 = 67    // 'C'
const WORKFLOW_OP_UPDATE: u8 = 87    // 'W'

const WORKFLOW_STATE_IDLE: u8 = 73       // 'I'
const WORKFLOW_STATE_WAITING: u8 = 87    // 'W'
const WORKFLOW_STATE_RUNNING: u8 = 82    // 'R'
const WORKFLOW_STATE_DELIVERING: u8 = 77 // 'M'
const WORKFLOW_STATE_DONE: u8 = 68       // 'D'
const WORKFLOW_STATE_CANCELLED: u8 = 67  // 'C'
const WORKFLOW_STATE_FAILED: u8 = 70     // 'F'
const WORKFLOW_STATE_OBJECT_UPDATED: u8 = 85   // 'U'
const WORKFLOW_STATE_OBJECT_REJECTED: u8 = 88  // 'X'
const WORKFLOW_STATE_OBJECT_CANCELLED: u8 = 75 // 'K'

const WORKFLOW_RESTART_NONE: u8 = 78       // 'N'
const WORKFLOW_RESTART_RESUMED: u8 = 82    // 'R'
const WORKFLOW_RESTART_CANCELLED: u8 = 67  // 'C'

const WORKFLOW_KIND_TASK: u8 = 0
const WORKFLOW_KIND_OBJECT_UPDATE: u8 = 1
const WORKFLOW_KIND_CONNECTION: u8 = 2
const WORKFLOW_TIMER_ID: u8 = 1
const WORKFLOW_DELIVERY_WOULDBLOCK: u8 = 2
const WORKFLOW_DELIVERY_EXHAUSTED: u8 = 7
const WORKFLOW_OBJECT_UPDATE_DELAY: u8 = 2
const WORKFLOW_JOURNAL_LANE_LEN: usize = 4
const WORKFLOW_CONNECTION_SLOT_MARKER: u8 = 128
const WORKFLOW_STATE_CONNECTION_EXECUTED: u8 = 69          // 'E'
const WORKFLOW_STATE_CONNECTION_CANCELLED: u8 = 89         // 'Y'
const WORKFLOW_STATE_CONNECTION_RESTART_CANCELLED: u8 = 90 // 'Z'

struct WorkflowServiceState {
    pid: u32
    kind: u8
    id: u8
    due: u8
    opcode: u8
    task: u8
    state: u8
    restart: u8
    generation: u8
}

struct WorkflowSnapshotRecord {
    kind: u8
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
    object_store: object_store_service.ObjectStoreServiceState
    completion: completion_mailbox_service.CompletionMailboxServiceState
}

struct WorkflowStepResult {
    workflow: WorkflowServiceState
    timer: timer_service.TimerServiceState
    task: task_service.TaskServiceState
    object_store: object_store_service.ObjectStoreServiceState
    journal: journal_service.JournalServiceState
    completion: completion_mailbox_service.CompletionMailboxServiceState
    effect: service_effect.Effect
}

struct WorkflowDeliveryResult {
    workflow: WorkflowServiceState
    completion: completion_mailbox_service.CompletionMailboxServiceState
}

struct WorkflowPayload {
    due: u8
    task: u8
}

struct WorkflowWaitingState {
    due: u8
}

struct WorkflowRunningState {
    task: u8
}

struct WorkflowDeliveryState {
    outcome: u8
    status: u8
}

struct WorkflowScheduleSpec {
    id: u8
    opcode: u8
    payload: WorkflowPayload
}

struct WorkflowCancelExecution {
    timer: timer_service.TimerServiceState
    task: task_service.TaskServiceState
    effect: service_effect.Effect
}

func workflow_advance_result(workflow: WorkflowServiceState, timer: timer_service.TimerServiceState, task: task_service.TaskServiceState, object_store: object_store_service.ObjectStoreServiceState, completion: completion_mailbox_service.CompletionMailboxServiceState) WorkflowAdvanceResult {
    return WorkflowAdvanceResult{ workflow: workflow, timer: timer, task: task, object_store: object_store, completion: completion }
}

func workflow_step_result(workflow: WorkflowServiceState, timer: timer_service.TimerServiceState, task: task_service.TaskServiceState, object_store: object_store_service.ObjectStoreServiceState, journal: journal_service.JournalServiceState, completion: completion_mailbox_service.CompletionMailboxServiceState, effect: service_effect.Effect) WorkflowStepResult {
    return WorkflowStepResult{ workflow: workflow, timer: timer, task: task, object_store: object_store, journal: journal, completion: completion, effect: effect }
}

func workflow_state_init(pid: u32) WorkflowServiceState {
    return WorkflowServiceState{ pid: pid, kind: WORKFLOW_KIND_TASK, id: 0, due: 0, opcode: 0, task: 0, state: WORKFLOW_STATE_IDLE, restart: WORKFLOW_RESTART_NONE, generation: 0 }
}

func workflow_state_raw(s: WorkflowServiceState, kind: u8, id: u8, due: u8, opcode: u8, task: u8, state: u8, restart: u8, generation: u8) WorkflowServiceState {
    return WorkflowServiceState{ pid: s.pid, kind: kind, id: id, due: due, opcode: opcode, task: task, state: state, restart: restart, generation: generation }
}

func workflow_payload(due: u8, task: u8) WorkflowPayload {
    return WorkflowPayload{ due: due, task: task }
}

func workflow_waiting_state(due: u8) WorkflowWaitingState {
    return WorkflowWaitingState{ due: due }
}

func workflow_running_state(task: u8) WorkflowRunningState {
    return WorkflowRunningState{ task: task }
}

func workflow_delivery_state(outcome: u8, status: u8) WorkflowDeliveryState {
    return WorkflowDeliveryState{ outcome: outcome, status: status }
}

func workflow_payload_wait(due: u8) WorkflowPayload {
    waiting := workflow_waiting_state(due)
    return workflow_payload(waiting.due, 0)
}

func workflow_payload_object_update(value: u8) WorkflowPayload {
    return workflow_payload(value, 0)
}

func workflow_payload_running(task: u8) WorkflowPayload {
    running := workflow_running_state(task)
    return workflow_payload(0, running.task)
}

func workflow_payload_delivery(outcome: u8, status: u8) WorkflowPayload {
    delivery := workflow_delivery_state(outcome, status)
    return workflow_payload(delivery.status, delivery.outcome)
}

func workflow_payload_terminal() WorkflowPayload {
    return workflow_payload(0, 0)
}

func workflow_payload_from_state(s: WorkflowServiceState) WorkflowPayload {
    return workflow_payload(s.due, s.task)
}

func workflow_record(s: WorkflowServiceState, kind: u8, id: u8, opcode: u8, payload: WorkflowPayload, state: u8, restart: u8, generation: u8) WorkflowServiceState {
    return workflow_state_raw(s, kind, id, payload.due, opcode, payload.task, state, restart, generation)
}

func workflow_schedule_spec(id: u8, opcode: u8, payload: WorkflowPayload) WorkflowScheduleSpec {
    return WorkflowScheduleSpec{ id: id, opcode: opcode, payload: payload }
}

func workflow_schedule_task_spec(id: u8, due: u8) WorkflowScheduleSpec {
    return workflow_schedule_spec(id, id, workflow_payload_wait(due))
}

func workflow_schedule_object_update_spec(name: u8, value: u8) WorkflowScheduleSpec {
    id := workflow_object_workflow_id(name)
    return workflow_schedule_spec(id, WORKFLOW_KIND_OBJECT_UPDATE, workflow_payload_object_update(value))
}

func workflow_schedule_connection_spec(id: u8, slot: u8, opcode: u8) WorkflowScheduleSpec {
    return workflow_schedule_spec(id, opcode, workflow_payload_wait(slot + WORKFLOW_CONNECTION_SLOT_MARKER))
}

func workflow_restate(s: WorkflowServiceState, payload: WorkflowPayload, state: u8, restart: u8, generation: u8) WorkflowServiceState {
    return workflow_record(s, s.kind, s.id, s.opcode, payload, state, restart, generation)
}

func workflow_snapshot_record(s: WorkflowServiceState) WorkflowSnapshotRecord {
    return WorkflowSnapshotRecord{ kind: s.kind, id: s.id, due: s.due, opcode: s.opcode, task: s.task, state: s.state, restart: s.restart, generation: s.generation }
}

func workflow_state_from_snapshot(pid: u32, snap: WorkflowSnapshotRecord) WorkflowServiceState {
    return WorkflowServiceState{ pid: pid, kind: snap.kind, id: snap.id, due: snap.due, opcode: snap.opcode, task: snap.task, state: snap.state, restart: snap.restart, generation: snap.generation }
}

func workflow_lane_bytes(s: WorkflowServiceState) [4]u8 {
    data: [4]u8 = primitives.zero_payload()
    data[0] = s.state
    if s.kind == WORKFLOW_KIND_CONNECTION {
        data[0] = s.state + WORKFLOW_CONNECTION_SLOT_MARKER
    }
    data[1] = s.id
    data[2] = s.due
    data[3] = s.opcode
    return data
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
    payload[2] = workflow_query_value(s)
    payload[3] = s.generation
    return service_effect.effect_reply(workflow_query_status(s), 4, payload)
}

func workflow_is_idle(s: WorkflowServiceState) bool {
    return s.state == WORKFLOW_STATE_IDLE
}

func workflow_is_waiting(s: WorkflowServiceState) bool {
    return s.state == WORKFLOW_STATE_WAITING
}

func workflow_is_running(s: WorkflowServiceState) bool {
    return s.state == WORKFLOW_STATE_RUNNING
}

func workflow_is_delivering(s: WorkflowServiceState) bool {
    return s.state == WORKFLOW_STATE_DELIVERING
}

func workflow_matches(s: WorkflowServiceState, id: u8) bool {
    if workflow_is_idle(s) {
        return false
    }
    return s.id == id
}

func workflow_is_active(s: WorkflowServiceState) bool {
    return workflow_is_waiting(s) || workflow_is_running(s) || workflow_is_delivering(s)
}

func workflow_is_task(s: WorkflowServiceState) bool {
    if !workflow_has_record(s) {
        return false
    }
    return s.kind == WORKFLOW_KIND_TASK
}

func workflow_is_object_update(s: WorkflowServiceState) bool {
    if !workflow_has_record(s) {
        return false
    }
    return s.kind == WORKFLOW_KIND_OBJECT_UPDATE
}

func workflow_is_connection(s: WorkflowServiceState) bool {
    if !workflow_has_record(s) {
        return false
    }
    return s.kind == WORKFLOW_KIND_CONNECTION
}

func workflow_has_record(s: WorkflowServiceState) bool {
    return !workflow_is_idle(s)
}

func workflow_object_workflow_id(name: u8) u8 {
    // Reserve workflow id 0 for the invalid or empty sentinel used on the wire.
    return name + 1
}

func workflow_connection_slot(s: WorkflowServiceState) u8 {
    return s.due - WORKFLOW_CONNECTION_SLOT_MARKER
}

func workflow_object_name(s: WorkflowServiceState) u8 {
    return s.id - 1
}

func workflow_wait_due(s: WorkflowServiceState) u8 {
    return workflow_waiting_state(s.due).due
}

func workflow_object_value(s: WorkflowServiceState) u8 {
    return s.due
}

func workflow_running_task_id(s: WorkflowServiceState) u8 {
    return workflow_running_state(s.task).task
}

func workflow_delivery_outcome(s: WorkflowServiceState) u8 {
    return workflow_delivery_state(s.task, s.due).outcome
}

func workflow_delivery_status(s: WorkflowServiceState) u8 {
    return workflow_delivery_state(s.task, s.due).status
}

func workflow_query_value(s: WorkflowServiceState) u8 {
    if workflow_is_running(s) {
        return workflow_running_task_id(s)
    }
    if workflow_is_delivering(s) {
        return workflow_delivery_outcome(s)
    }
    return 0
}

func workflow_prepare_schedule(s: WorkflowServiceState, generation: u8) WorkflowServiceState {
    if workflow_is_active(s) {
        return s
    }
    return workflow_restate(s, workflow_payload_from_state(s), s.state, WORKFLOW_RESTART_NONE, generation)
}

func workflow_can_schedule(s: WorkflowServiceState) bool {
    return !workflow_is_active(s)
}

func workflow_schedule_with_spec(s: WorkflowServiceState, spec: WorkflowScheduleSpec) WorkflowResult {
    next := workflow_record(s, WORKFLOW_KIND_TASK, spec.id, spec.opcode, spec.payload, WORKFLOW_STATE_WAITING, WORKFLOW_RESTART_NONE, s.generation)
    return WorkflowResult{ state: next, effect: workflow_schedule_effect(spec.id, WORKFLOW_STATE_WAITING) }
}

func workflow_schedule(s: WorkflowServiceState, id: u8, due: u8) WorkflowResult {
    if id == 0 {
        return WorkflowResult{ state: s, effect: workflow_reply_invalid() }
    }
    if !workflow_can_schedule(s) {
        return WorkflowResult{ state: s, effect: workflow_reply_invalid() }
    }
    return workflow_schedule_with_spec(s, workflow_schedule_task_spec(id, due))
}

func workflow_schedule_object_update(s: WorkflowServiceState, name: u8, value: u8) WorkflowResult {
    if name == 255 {
        return WorkflowResult{ state: s, effect: workflow_reply_invalid() }
    }
    if !workflow_can_schedule(s) {
        return WorkflowResult{ state: s, effect: workflow_reply_invalid() }
    }
    spec := workflow_schedule_object_update_spec(name, value)
    next := workflow_record(s, WORKFLOW_KIND_OBJECT_UPDATE, spec.id, spec.opcode, spec.payload, WORKFLOW_STATE_WAITING, WORKFLOW_RESTART_NONE, s.generation)
    return WorkflowResult{ state: next, effect: workflow_schedule_effect(spec.id, WORKFLOW_STATE_WAITING) }
}

func workflow_schedule_connection(s: WorkflowServiceState, id: u8, slot: u8, opcode: u8) WorkflowResult {
    if id == 0 || !connection_service.connection_slot_valid(connection_service.connection_find(slot)) || opcode == 0 {
        return WorkflowResult{ state: s, effect: workflow_reply_invalid() }
    }
    if !workflow_can_schedule(s) {
        return WorkflowResult{ state: s, effect: workflow_reply_invalid() }
    }
    spec := workflow_schedule_connection_spec(id, slot, opcode)
    next := workflow_record(s, WORKFLOW_KIND_CONNECTION, spec.id, spec.opcode, spec.payload, WORKFLOW_STATE_WAITING, WORKFLOW_RESTART_NONE, s.generation)
    return WorkflowResult{ state: next, effect: workflow_schedule_effect(spec.id, WORKFLOW_STATE_WAITING) }
}

func workflow_can_cancel(s: WorkflowServiceState, id: u8) bool {
    return workflow_matches(s, id) && workflow_is_active(s)
}

func workflow_cancel(s: WorkflowServiceState, id: u8) WorkflowResult {
    if !workflow_can_cancel(s, id) {
        return WorkflowResult{ state: s, effect: workflow_reply_invalid() }
    }
    next := workflow_restate(s, workflow_payload_terminal(), workflow_cancelled_outcome(s), s.restart, s.generation)
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
    case WORKFLOW_OP_UPDATE:
        if m.payload_len != 3 {
            return WorkflowResult{ state: s, effect: workflow_reply_invalid() }
        }
        return workflow_schedule_object_update(s, m.payload[1], m.payload[2])
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

func workflow_waiting_task(s: WorkflowServiceState, due: u8, restart: u8, generation: u8) WorkflowServiceState {
    return workflow_restate(s, workflow_payload_wait(due), WORKFLOW_STATE_WAITING, restart, generation)
}

func workflow_waiting_object_update(s: WorkflowServiceState, restart: u8, generation: u8) WorkflowServiceState {
    return workflow_restate(s, workflow_payload_object_update(workflow_object_value(s)), WORKFLOW_STATE_WAITING, restart, generation)
}

func workflow_waiting_connection(s: WorkflowServiceState, slot: u8, restart: u8, generation: u8) WorkflowServiceState {
    return workflow_restate(s, workflow_payload_wait(slot + WORKFLOW_CONNECTION_SLOT_MARKER), WORKFLOW_STATE_WAITING, restart, generation)
}

func workflow_running_task(s: WorkflowServiceState, task: u8) WorkflowServiceState {
    return workflow_restate(s, workflow_payload_running(task), WORKFLOW_STATE_RUNNING, s.restart, s.generation)
}

func workflow_running_connection(s: WorkflowServiceState, slot: u8, task: u8) WorkflowServiceState {
    return workflow_restate(s, workflow_payload(slot + WORKFLOW_CONNECTION_SLOT_MARKER, task), WORKFLOW_STATE_RUNNING, s.restart, s.generation)
}

func workflow_pending_delivery(s: WorkflowServiceState, outcome: u8, status: u8) WorkflowServiceState {
    return workflow_restate(s, workflow_payload_delivery(outcome, status), WORKFLOW_STATE_DELIVERING, s.restart, s.generation)
}

func workflow_terminal(s: WorkflowServiceState, state: u8) WorkflowServiceState {
    return workflow_restate(s, workflow_payload_terminal(), state, s.restart, s.generation)
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

func workflow_connection_active(s: WorkflowServiceState, connection: connection_service.ConnectionServiceState) bool {
    return connection_service.connection_request_active(connection, workflow_connection_slot(s), s.id)
}

func workflow_connection_cancel_result(s: WorkflowServiceState, timer: timer_service.TimerServiceState, task: task_service.TaskServiceState) WorkflowCancelExecution {
    if workflow_is_waiting(s) {
        cancel_timer := timer_service.timer_cancel(timer, WORKFLOW_TIMER_ID)
        return WorkflowCancelExecution{ timer: cancel_timer.state, task: task, effect: cancel_timer.effect }
    }
    cancel_task := task_service.task_cancel(task, workflow_running_task_id(s))
    return WorkflowCancelExecution{ timer: timer, task: cancel_task.state, effect: cancel_task.effect }
}

func workflow_connection_cancelled_state(s: WorkflowServiceState) u8 {
    if s.restart == WORKFLOW_RESTART_CANCELLED {
        return WORKFLOW_STATE_CONNECTION_RESTART_CANCELLED
    }
    return WORKFLOW_STATE_CONNECTION_CANCELLED
}

func workflow_schedule_due_for_message(m: service_effect.Message) u8 {
    if m.payload[0] == WORKFLOW_OP_UPDATE {
        return WORKFLOW_OBJECT_UPDATE_DELAY
    }
    return m.payload[2]
}

func workflow_restart_timer_due(s: WorkflowServiceState) u8 {
    if workflow_is_object_update(s) {
        return WORKFLOW_OBJECT_UPDATE_DELAY
    }
    if workflow_wait_due(s) == 0 {
        return 1
    }
    return workflow_wait_due(s) + 1
}

func workflow_query_status(s: WorkflowServiceState) syscall.SyscallStatus {
    if !workflow_is_delivering(s) {
        return syscall.SyscallStatus.Ok
    }
    if workflow_delivery_status(s) == WORKFLOW_DELIVERY_WOULDBLOCK {
        return syscall.SyscallStatus.WouldBlock
    }
    if workflow_delivery_status(s) == WORKFLOW_DELIVERY_EXHAUSTED {
        return syscall.SyscallStatus.Exhausted
    }
    return syscall.SyscallStatus.InvalidArgument
}

func workflow_cancelled_outcome(s: WorkflowServiceState) u8 {
    if workflow_is_connection(s) {
        return workflow_connection_cancelled_state(s)
    }
    if workflow_is_object_update(s) {
        return WORKFLOW_STATE_OBJECT_CANCELLED
    }
    return WORKFLOW_STATE_CANCELLED
}

func workflow_object_update_outcome(effect: service_effect.Effect) u8 {
    if service_effect.effect_reply_status(effect) == syscall.SyscallStatus.Ok {
        return WORKFLOW_STATE_OBJECT_UPDATED
    }
    return WORKFLOW_STATE_OBJECT_REJECTED
}

func workflow_deliver_completion(s: WorkflowServiceState, completion: completion_mailbox_service.CompletionMailboxServiceState, outcome: u8) WorkflowDeliveryResult {
    queued := completion_mailbox_service.completion_mailbox_enqueue(completion, s.id, outcome, s.restart, s.generation)
    if service_effect.effect_reply_status(queued.effect) == syscall.SyscallStatus.Ok {
        return WorkflowDeliveryResult{ workflow: workflow_terminal(s, outcome), completion: queued.state }
    }
    status := workflow_delivery_status_code(service_effect.effect_reply_status(queued.effect))
    return WorkflowDeliveryResult{ workflow: workflow_pending_delivery(s, outcome, status), completion: queued.state }
}

func workflow_advance_waiting(s: WorkflowServiceState, timer: timer_service.TimerServiceState, task: task_service.TaskServiceState, object_store: object_store_service.ObjectStoreServiceState, completion: completion_mailbox_service.CompletionMailboxServiceState, connection: connection_service.ConnectionServiceState) WorkflowAdvanceResult {
    next_task: task_service.TaskServiceState = task
    next_object_store: object_store_service.ObjectStoreServiceState = object_store
    next_completion: completion_mailbox_service.CompletionMailboxServiceState = completion

    if workflow_is_connection(s) && !workflow_connection_active(s, connection) {
        connection_cancelled_delivery := workflow_deliver_completion(s with { restart: WORKFLOW_RESTART_NONE }, next_completion, WORKFLOW_STATE_CONNECTION_CANCELLED)
        return workflow_advance_result(connection_cancelled_delivery.workflow, timer, next_task, next_object_store, connection_cancelled_delivery.completion)
    }

    idx := timer_service.timer_find(timer, WORKFLOW_TIMER_ID)
    if idx >= timer_service.TIMER_CAPACITY {
        missing_delivery := workflow_deliver_completion(s, next_completion, workflow_cancelled_outcome(s))
        return workflow_advance_result(missing_delivery.workflow, timer, next_task, next_object_store, missing_delivery.completion)
    }

    due := timer.due[idx]
    if timer.state[idx] == timer_service.TIMER_STATE_ACTIVE {
        if workflow_is_connection(s) {
            return workflow_advance_result(workflow_waiting_connection(s, workflow_connection_slot(s), s.restart, s.generation), timer, next_task, next_object_store, next_completion)
        }
        if workflow_is_object_update(s) {
            return workflow_advance_result(workflow_waiting_object_update(s, s.restart, s.generation), timer, next_task, next_object_store, next_completion)
        }
        return workflow_advance_result(workflow_waiting_task(s, due, s.restart, s.generation), timer, next_task, next_object_store, next_completion)
    }
    if timer.state[idx] == timer_service.TIMER_STATE_CANCELLED {
        timer_cancelled_delivery := workflow_deliver_completion(s, next_completion, workflow_cancelled_outcome(s))
        return workflow_advance_result(timer_cancelled_delivery.workflow, timer, next_task, next_object_store, timer_cancelled_delivery.completion)
    }
    if timer.state[idx] != timer_service.TIMER_STATE_EXPIRED {
        if workflow_is_object_update(s) {
            invalid_object := workflow_deliver_completion(s, next_completion, WORKFLOW_STATE_OBJECT_CANCELLED)
            return workflow_advance_result(invalid_object.workflow, timer, next_task, next_object_store, invalid_object.completion)
        }
        invalid_timer := workflow_deliver_completion(s, next_completion, WORKFLOW_STATE_FAILED)
        return workflow_advance_result(invalid_timer.workflow, timer, next_task, next_object_store, invalid_timer.completion)
    }

    if workflow_is_object_update(s) {
        update := object_store_service.object_update(next_object_store, workflow_object_name(s), workflow_object_value(s))
        next_object_store = update.state
        update_delivery := workflow_deliver_completion(s, next_completion, workflow_object_update_outcome(update.effect))
        return workflow_advance_result(update_delivery.workflow, timer, next_task, next_object_store, update_delivery.completion)
    }

    if workflow_is_connection(s) {
        connection_submit := task_service.task_submit(task, s.opcode)
        next_task = connection_submit.state
        if service_effect.effect_reply_status(connection_submit.effect) != syscall.SyscallStatus.Ok {
            connection_submit_failed := workflow_deliver_completion(s, next_completion, WORKFLOW_STATE_CONNECTION_CANCELLED)
            return workflow_advance_result(connection_submit_failed.workflow, timer, next_task, next_object_store, connection_submit_failed.completion)
        }

        connection_payload := service_effect.effect_reply_payload(connection_submit.effect)
        if connection_payload[1] == task_service.TASK_STATE_FAILED {
            immediate_cancel := workflow_deliver_completion(s, next_completion, WORKFLOW_STATE_CONNECTION_CANCELLED)
            return workflow_advance_result(immediate_cancel.workflow, timer, next_task, next_object_store, immediate_cancel.completion)
        }

        return workflow_advance_result(workflow_running_connection(s, workflow_connection_slot(s), connection_payload[0]), timer, next_task, next_object_store, next_completion)
    }

    submit := task_service.task_submit(task, s.opcode)
    next_task = submit.state
    if service_effect.effect_reply_status(submit.effect) != syscall.SyscallStatus.Ok {
        submit_failed := workflow_deliver_completion(s, next_completion, WORKFLOW_STATE_FAILED)
        return workflow_advance_result(submit_failed.workflow, timer, next_task, next_object_store, submit_failed.completion)
    }

    payload := service_effect.effect_reply_payload(submit.effect)
    if payload[1] == task_service.TASK_STATE_FAILED {
        immediate_failed := workflow_deliver_completion(s, next_completion, WORKFLOW_STATE_FAILED)
        return workflow_advance_result(immediate_failed.workflow, timer, next_task, next_object_store, immediate_failed.completion)
    }

    return workflow_advance_result(workflow_running_task(s, payload[0]), timer, next_task, next_object_store, next_completion)
}

func workflow_advance_running(s: WorkflowServiceState, timer: timer_service.TimerServiceState, task: task_service.TaskServiceState, object_store: object_store_service.ObjectStoreServiceState, completion: completion_mailbox_service.CompletionMailboxServiceState, connection: connection_service.ConnectionServiceState) WorkflowAdvanceResult {
    next_task: task_service.TaskServiceState = task
    next_object_store: object_store_service.ObjectStoreServiceState = object_store
    next_completion: completion_mailbox_service.CompletionMailboxServiceState = completion
    running := workflow_running_state(workflow_running_task_id(s))

    if workflow_is_connection(s) && !workflow_connection_active(s, connection) {
        cancelled_execution := workflow_connection_cancel_result(s, timer, task)
        next_task = cancelled_execution.task
        connection_cancelled_delivery := workflow_deliver_completion(s with { restart: WORKFLOW_RESTART_NONE }, next_completion, WORKFLOW_STATE_CONNECTION_CANCELLED)
        return workflow_advance_result(connection_cancelled_delivery.workflow, cancelled_execution.timer, next_task, next_object_store, connection_cancelled_delivery.completion)
    }

    complete := task_service.task_complete(task, running.task)
    next_task = complete.state
    if service_effect.effect_reply_status(complete.effect) == syscall.SyscallStatus.Ok {
        if workflow_is_connection(s) {
            connection_done_delivery := workflow_deliver_completion(s, next_completion, WORKFLOW_STATE_CONNECTION_EXECUTED)
            return workflow_advance_result(connection_done_delivery.workflow, timer, next_task, next_object_store, connection_done_delivery.completion)
        }
        task_done_delivery := workflow_deliver_completion(s, next_completion, WORKFLOW_STATE_DONE)
        return workflow_advance_result(task_done_delivery.workflow, timer, next_task, next_object_store, task_done_delivery.completion)
    }

    query := task_service.task_query(next_task, running.task)
    next_task = query.state
    if service_effect.effect_reply_status(query.effect) != syscall.SyscallStatus.Ok {
        query_failed := workflow_deliver_completion(s, next_completion, WORKFLOW_STATE_FAILED)
        return workflow_advance_result(query_failed.workflow, timer, next_task, next_object_store, query_failed.completion)
    }

    query_payload := service_effect.effect_reply_payload(query.effect)
    if query_payload[0] == task_service.TASK_STATE_CANCELLED {
        if workflow_is_connection(s) {
            connection_task_cancelled := workflow_deliver_completion(s, next_completion, WORKFLOW_STATE_CONNECTION_CANCELLED)
            return workflow_advance_result(connection_task_cancelled.workflow, timer, next_task, next_object_store, connection_task_cancelled.completion)
        }
        task_cancelled_delivery := workflow_deliver_completion(s, next_completion, WORKFLOW_STATE_CANCELLED)
        return workflow_advance_result(task_cancelled_delivery.workflow, timer, next_task, next_object_store, task_cancelled_delivery.completion)
    }
    if query_payload[0] == task_service.TASK_STATE_DONE {
        if workflow_is_connection(s) {
            connection_query_done := workflow_deliver_completion(s, next_completion, WORKFLOW_STATE_CONNECTION_EXECUTED)
            return workflow_advance_result(connection_query_done.workflow, timer, next_task, next_object_store, connection_query_done.completion)
        }
        task_query_done := workflow_deliver_completion(s, next_completion, WORKFLOW_STATE_DONE)
        return workflow_advance_result(task_query_done.workflow, timer, next_task, next_object_store, task_query_done.completion)
    }
    if query_payload[0] == task_service.TASK_STATE_FAILED {
        if workflow_is_connection(s) {
            connection_task_failed := workflow_deliver_completion(s, next_completion, WORKFLOW_STATE_CONNECTION_CANCELLED)
            return workflow_advance_result(connection_task_failed.workflow, timer, next_task, next_object_store, connection_task_failed.completion)
        }
        task_failed_delivery := workflow_deliver_completion(s, next_completion, WORKFLOW_STATE_FAILED)
        return workflow_advance_result(task_failed_delivery.workflow, timer, next_task, next_object_store, task_failed_delivery.completion)
    }

    return workflow_advance_result(s, timer, next_task, next_object_store, next_completion)
}

func workflow_advance_delivering(s: WorkflowServiceState, timer: timer_service.TimerServiceState, task: task_service.TaskServiceState, object_store: object_store_service.ObjectStoreServiceState, completion: completion_mailbox_service.CompletionMailboxServiceState) WorkflowAdvanceResult {
    delivery := workflow_delivery_state(workflow_delivery_outcome(s), workflow_delivery_status(s))
    retry_delivery := workflow_deliver_completion(s, completion, delivery.outcome)
    return workflow_advance_result(retry_delivery.workflow, timer, task, object_store, retry_delivery.completion)
}

func workflow_advance(s: WorkflowServiceState, timer: timer_service.TimerServiceState, task: task_service.TaskServiceState, object_store: object_store_service.ObjectStoreServiceState, completion: completion_mailbox_service.CompletionMailboxServiceState, connection: connection_service.ConnectionServiceState) WorkflowAdvanceResult {
    if workflow_is_waiting(s) {
        return workflow_advance_waiting(s, timer, task, object_store, completion, connection)
    }

    if workflow_is_running(s) {
        return workflow_advance_running(s, timer, task, object_store, completion, connection)
    }

    if workflow_is_delivering(s) {
        return workflow_advance_delivering(s, timer, task, object_store, completion)
    }

    return workflow_advance_result(s, timer, task, object_store, completion)
}

func workflow_sync_journal(journal: journal_service.JournalServiceState, s: WorkflowServiceState) journal_service.JournalServiceState {
    lane := journal_service.lane_init(journal_service.JOURNAL_LANE_B)
    if workflow_has_record(s) || s.restart != WORKFLOW_RESTART_NONE {
        lane = journal_service.JournalLane{ name: journal_service.JOURNAL_LANE_B, len: WORKFLOW_JOURNAL_LANE_LEN, data: workflow_lane_bytes(s) }
    }
    return journal_service.journalwith(journal, journal.lane0, lane)
}

func workflow_step_with_synced_journal(workflow: WorkflowServiceState, timer: timer_service.TimerServiceState, task: task_service.TaskServiceState, object_store: object_store_service.ObjectStoreServiceState, journal: journal_service.JournalServiceState, completion: completion_mailbox_service.CompletionMailboxServiceState, effect: service_effect.Effect) WorkflowStepResult {
    return workflow_step_result(workflow, timer, task, object_store, workflow_sync_journal(journal, workflow), completion, effect)
}

func workflow_step_schedule(workflow: WorkflowServiceState, timer: timer_service.TimerServiceState, task: task_service.TaskServiceState, object_store: object_store_service.ObjectStoreServiceState, journal: journal_service.JournalServiceState, completion: completion_mailbox_service.CompletionMailboxServiceState, msg: service_effect.Message, generation: u8) WorkflowStepResult {
    prepared := workflow_prepare_schedule(workflow, generation)
    scheduled := handle(prepared, msg)
    if service_effect.effect_reply_status(scheduled.effect) != syscall.SyscallStatus.Ok {
        return workflow_step_with_synced_journal(workflow, timer, task, object_store, journal, completion, scheduled.effect)
    }

    create := timer_service.timer_create(timer, WORKFLOW_TIMER_ID, workflow_schedule_due_for_message(msg))
    next_timer := create.state
    if service_effect.effect_reply_status(create.effect) != syscall.SyscallStatus.Ok {
        return workflow_step_with_synced_journal(workflow, next_timer, task, object_store, journal, completion, create.effect)
    }

    return workflow_step_with_synced_journal(scheduled.state, next_timer, task, object_store, journal, completion, scheduled.effect)
}

func workflow_step_schedule_connection(workflow: WorkflowServiceState, timer: timer_service.TimerServiceState, task: task_service.TaskServiceState, object_store: object_store_service.ObjectStoreServiceState, journal: journal_service.JournalServiceState, completion: completion_mailbox_service.CompletionMailboxServiceState, id: u8, slot: u8, opcode: u8, generation: u8) WorkflowStepResult {
    prepared := workflow_prepare_schedule(workflow, generation)
    scheduled := workflow_schedule_connection(prepared, id, slot, opcode)
    if service_effect.effect_reply_status(scheduled.effect) != syscall.SyscallStatus.Ok {
        return workflow_step_with_synced_journal(workflow, timer, task, object_store, journal, completion, scheduled.effect)
    }

    create := timer_service.timer_create(timer, WORKFLOW_TIMER_ID, 2)
    next_timer := create.state
    if service_effect.effect_reply_status(create.effect) != syscall.SyscallStatus.Ok {
        return workflow_step_with_synced_journal(workflow, next_timer, task, object_store, journal, completion, create.effect)
    }

    return workflow_step_with_synced_journal(scheduled.state, next_timer, task, object_store, journal, completion, scheduled.effect)
}

func workflow_execute_cancel(workflow: WorkflowServiceState, timer: timer_service.TimerServiceState, task: task_service.TaskServiceState) WorkflowCancelExecution {
    if workflow_is_waiting(workflow) {
        cancel_timer := timer_service.timer_cancel(timer, WORKFLOW_TIMER_ID)
        return WorkflowCancelExecution{ timer: cancel_timer.state, task: task, effect: cancel_timer.effect }
    }

    cancel_task := task_service.task_cancel(task, workflow_running_task_id(workflow))
    return WorkflowCancelExecution{ timer: timer, task: cancel_task.state, effect: cancel_task.effect }
}

func workflow_step_cancel(workflow: WorkflowServiceState, timer: timer_service.TimerServiceState, task: task_service.TaskServiceState, object_store: object_store_service.ObjectStoreServiceState, journal: journal_service.JournalServiceState, completion: completion_mailbox_service.CompletionMailboxServiceState, msg: service_effect.Message) WorkflowStepResult {
    if msg.payload_len != 2 || !workflow_can_cancel(workflow, msg.payload[1]) {
        return workflow_step_with_synced_journal(workflow, timer, task, object_store, journal, completion, workflow_reply_invalid())
    }

    cancelled_execution := workflow_execute_cancel(workflow, timer, task)
    if service_effect.effect_reply_status(cancelled_execution.effect) != syscall.SyscallStatus.Ok {
        return workflow_step_with_synced_journal(workflow, cancelled_execution.timer, cancelled_execution.task, object_store, journal, completion, cancelled_execution.effect)
    }

    cancelled := handle(workflow, msg)
    return workflow_step_with_synced_journal(cancelled.state, cancelled_execution.timer, cancelled_execution.task, object_store, journal, completion, cancelled.effect)
}

func workflow_lane_matches(s: WorkflowServiceState, lane: journal_service.JournalLane) bool {
    if lane.name != journal_service.JOURNAL_LANE_B || lane.len != WORKFLOW_JOURNAL_LANE_LEN {
        return false
    }
    bytes := workflow_lane_bytes(s)
    for i in 0..WORKFLOW_JOURNAL_LANE_LEN {
        if lane.data[i] != bytes[i] {
            return false
        }
    }
    return true
}

func workflow_restart_reload(pid: u32, snap: WorkflowSnapshotRecord, lane: journal_service.JournalLane, generation: u8, keep: bool, connection: connection_service.ConnectionServiceState) WorkflowServiceState {
    current := workflow_state_from_snapshot(pid, snap)
    if !workflow_has_record(current) {
        return current
    }
    if workflow_is_delivering(current) {
        delivery := workflow_delivery_state(workflow_delivery_outcome(current), workflow_delivery_status(current))
        if !keep {
            return workflow_restate(current, workflow_payload_delivery(delivery.outcome, delivery.status), current.state, WORKFLOW_RESTART_CANCELLED, generation)
        }
        return workflow_restate(current, workflow_payload_delivery(delivery.outcome, delivery.status), current.state, WORKFLOW_RESTART_RESUMED, generation)
    }
    if !workflow_is_active(current) {
        return workflow_restate(current, workflow_payload_from_state(current), current.state, WORKFLOW_RESTART_NONE, current.generation)
    }
    if !keep || current.generation != generation || !workflow_lane_matches(current, lane) {
        if workflow_is_connection(current) {
            return workflow_restate(current, workflow_payload_delivery(WORKFLOW_STATE_CONNECTION_RESTART_CANCELLED, 0), WORKFLOW_STATE_DELIVERING, WORKFLOW_RESTART_CANCELLED, generation)
        }
        if workflow_is_object_update(current) {
            return workflow_restate(current, workflow_payload_delivery(WORKFLOW_STATE_OBJECT_CANCELLED, 0), WORKFLOW_STATE_DELIVERING, WORKFLOW_RESTART_CANCELLED, generation)
        }
        return workflow_restate(current, workflow_payload_terminal(), WORKFLOW_STATE_CANCELLED, WORKFLOW_RESTART_CANCELLED, generation)
    }
    if workflow_is_connection(current) && !workflow_connection_active(current, connection) {
        return workflow_restate(current, workflow_payload_delivery(WORKFLOW_STATE_CONNECTION_RESTART_CANCELLED, 0), WORKFLOW_STATE_DELIVERING, WORKFLOW_RESTART_CANCELLED, generation)
    }
    if workflow_is_running(current) {
        if workflow_is_connection(current) {
            return workflow_waiting_connection(current, workflow_connection_slot(current), WORKFLOW_RESTART_RESUMED, generation)
        }
        return workflow_waiting_task(current, 0, WORKFLOW_RESTART_RESUMED, generation)
    }
    if workflow_is_object_update(current) {
        return workflow_waiting_object_update(current, WORKFLOW_RESTART_RESUMED, generation)
    }
    if workflow_is_connection(current) {
        return workflow_waiting_connection(current, workflow_connection_slot(current), WORKFLOW_RESTART_RESUMED, generation)
    }
    return workflow_waiting_task(current, workflow_wait_due(current), WORKFLOW_RESTART_RESUMED, generation)
}

func step(s: WorkflowServiceState, timer: timer_service.TimerServiceState, task: task_service.TaskServiceState, object_store: object_store_service.ObjectStoreServiceState, journal: journal_service.JournalServiceState, completion: completion_mailbox_service.CompletionMailboxServiceState, connection: connection_service.ConnectionServiceState, msg: service_effect.Message, generation: u8) WorkflowStepResult {
    ticked_timer := timer_service.timer_tick(timer)
    advanced := workflow_advance(s, ticked_timer, task, object_store, completion, connection)
    next_workflow := advanced.workflow
    next_timer := advanced.timer
    next_task := advanced.task
    next_object_store := advanced.object_store
    next_completion := advanced.completion

    if msg.payload_len == 0 {
        return workflow_step_with_synced_journal(next_workflow, next_timer, next_task, next_object_store, journal, next_completion, workflow_reply_invalid())
    }

    if msg.payload[0] == WORKFLOW_OP_SCHEDULE || msg.payload[0] == WORKFLOW_OP_UPDATE {
        return workflow_step_schedule(next_workflow, next_timer, next_task, next_object_store, journal, next_completion, msg, generation)
    }

    if msg.payload[0] == WORKFLOW_OP_CANCEL {
        return workflow_step_cancel(next_workflow, next_timer, next_task, next_object_store, journal, next_completion, msg)
    }

    handled := handle(next_workflow, msg)
    return workflow_step_with_synced_journal(handled.state, next_timer, next_task, next_object_store, journal, next_completion, handled.effect)
}

func workflow_active_count(s: WorkflowServiceState) usize {
    if workflow_is_active(s) {
        return 1
    }
    return 0
}