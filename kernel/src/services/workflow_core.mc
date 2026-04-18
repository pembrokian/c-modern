import journal_service
import completion_mailbox_service
import connection_service
import lease_service
import object_store_service
import primitives
import service_effect
import syscall
import task_service
import ticket_service
import timer_service
import update_store_service

const WORKFLOW_OP_SCHEDULE: u8 = 83  // 'S'
const WORKFLOW_OP_QUERY: u8 = 81     // 'Q'
const WORKFLOW_OP_CANCEL: u8 = 67    // 'C'
const WORKFLOW_OP_UPDATE: u8 = 87    // 'W'
const WORKFLOW_OP_APPLY_UPDATE: u8 = 65 // 'A'
const WORKFLOW_OP_APPLY_UPDATE_LEASE: u8 = 76 // 'L'

const WORKFLOW_STATE_IDLE: u8 = 73       // 'I'
const WORKFLOW_STATE_WAITING: u8 = 87    // 'W'
const WORKFLOW_STATE_RUNNING: u8 = 82    // 'R'
const WORKFLOW_STATE_DELIVERING: u8 = 77 // 'M'
const WORKFLOW_STATE_DONE: u8 = 68       // 'D'
const WORKFLOW_STATE_CANCELLED: u8 = 67  // 'C'
const WORKFLOW_STATE_FAILED: u8 = 70     // 'F'
const WORKFLOW_STATE_OBJECT_UPDATED: u8 = 85   // 'U'
const WORKFLOW_STATE_OBJECT_CONFLICT: u8 = 86  // 'V'
const WORKFLOW_STATE_OBJECT_REJECTED: u8 = 88  // 'X'
const WORKFLOW_STATE_OBJECT_CANCELLED: u8 = 75 // 'K'
const WORKFLOW_STATE_UPDATE_APPLIED: u8 = 65   // 'A'
const WORKFLOW_STATE_UPDATE_REJECTED: u8 = 88  // 'X'
const WORKFLOW_STATE_UPDATE_CANCELLED: u8 = 75 // 'K'
const WORKFLOW_STATE_UPDATE_LEASE_INVALID: u8 = 74  // 'J'
const WORKFLOW_STATE_UPDATE_LEASE_STALE: u8 = 83    // 'S'
const WORKFLOW_STATE_UPDATE_LEASE_CONSUMED: u8 = 80 // 'P'

const WORKFLOW_RESTART_NONE: u8 = 78       // 'N'
const WORKFLOW_RESTART_RESUMED: u8 = 82    // 'R'
const WORKFLOW_RESTART_CANCELLED: u8 = 67  // 'C'

const WORKFLOW_KIND_TASK: u8 = 0
const WORKFLOW_KIND_OBJECT_UPDATE: u8 = 1
const WORKFLOW_KIND_CONNECTION: u8 = 2
const WORKFLOW_KIND_UPDATE_APPLY: u8 = 3
const WORKFLOW_TIMER_ID: u8 = 1
const WORKFLOW_DELIVERY_WOULDBLOCK: u8 = 2
const WORKFLOW_DELIVERY_EXHAUSTED: u8 = 7
const WORKFLOW_OBJECT_UPDATE_DELAY: u8 = 2
const WORKFLOW_UPDATE_APPLY_DELAY: u8 = 2
const WORKFLOW_JOURNAL_LANE_LEN: usize = 4
const WORKFLOW_CONNECTION_SLOT_MARKER: u8 = 128
const WORKFLOW_UPDATE_APPLY_ID: u8 = 200
const WORKFLOW_STATE_CONNECTION_EXECUTED: u8 = 69          // 'E'
const WORKFLOW_STATE_CONNECTION_CANCELLED: u8 = 89         // 'Y'
const WORKFLOW_STATE_CONNECTION_RESTART_CANCELLED: u8 = 90 // 'Z'
const WORKFLOW_STATE_CONNECTION_TICKET_USED: u8 = 86       // 'V'
const WORKFLOW_STATE_CONNECTION_TICKET_INVALID: u8 = 73    // 'I'
const WORKFLOW_STATE_CONNECTION_TICKET_STALE: u8 = 83      // 'S'
const WORKFLOW_STATE_CONNECTION_TICKET_CONSUMED: u8 = 80   // 'P'

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
    update_store: update_store_service.UpdateStoreServiceState
    lease: lease_service.LeaseServiceState
    ticket: ticket_service.TicketServiceState
    completion: completion_mailbox_service.CompletionMailboxServiceState
}

struct WorkflowStepResult {
    workflow: WorkflowServiceState
    timer: timer_service.TimerServiceState
    task: task_service.TaskServiceState
    object_store: object_store_service.ObjectStoreServiceState
    update_store: update_store_service.UpdateStoreServiceState
    journal: journal_service.JournalServiceState
    lease: lease_service.LeaseServiceState
    ticket: ticket_service.TicketServiceState
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

struct WorkflowDelegatedConnectionResult {
    workflow: WorkflowServiceState
    lease: lease_service.LeaseServiceState
    ticket: ticket_service.TicketServiceState
    completion: completion_mailbox_service.CompletionMailboxServiceState
}

struct WorkflowDelegatedUpdateApplyResult {
    workflow: WorkflowServiceState
    lease: lease_service.LeaseServiceState
    update_store: update_store_service.UpdateStoreServiceState
    completion: completion_mailbox_service.CompletionMailboxServiceState
}

func workflow_advance_result(workflow: WorkflowServiceState, timer: timer_service.TimerServiceState, task: task_service.TaskServiceState, object_store: object_store_service.ObjectStoreServiceState, update_store: update_store_service.UpdateStoreServiceState, lease: lease_service.LeaseServiceState, ticket: ticket_service.TicketServiceState, completion: completion_mailbox_service.CompletionMailboxServiceState) WorkflowAdvanceResult {
    return WorkflowAdvanceResult{ workflow: workflow, timer: timer, task: task, object_store: object_store, update_store: update_store, lease: lease, ticket: ticket, completion: completion }
}

func workflow_step_result(workflow: WorkflowServiceState, timer: timer_service.TimerServiceState, task: task_service.TaskServiceState, object_store: object_store_service.ObjectStoreServiceState, update_store: update_store_service.UpdateStoreServiceState, journal: journal_service.JournalServiceState, lease: lease_service.LeaseServiceState, ticket: ticket_service.TicketServiceState, completion: completion_mailbox_service.CompletionMailboxServiceState, effect: service_effect.Effect) WorkflowStepResult {
    return WorkflowStepResult{ workflow: workflow, timer: timer, task: task, object_store: object_store, update_store: update_store, journal: journal, lease: lease, ticket: ticket, completion: completion, effect: effect }
}

func workflow_delegated_connection_result(workflow: WorkflowServiceState, lease: lease_service.LeaseServiceState, ticket: ticket_service.TicketServiceState, completion: completion_mailbox_service.CompletionMailboxServiceState) WorkflowDelegatedConnectionResult {
    return WorkflowDelegatedConnectionResult{ workflow: workflow, lease: lease, ticket: ticket, completion: completion }
}

func workflow_delegated_update_apply_result(workflow: WorkflowServiceState, lease: lease_service.LeaseServiceState, update_store: update_store_service.UpdateStoreServiceState, completion: completion_mailbox_service.CompletionMailboxServiceState) WorkflowDelegatedUpdateApplyResult {
    return WorkflowDelegatedUpdateApplyResult{ workflow: workflow, lease: lease, update_store: update_store, completion: completion }
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
    return workflow_schedule_spec(id, 0, workflow_payload_object_update(value))
}

func workflow_schedule_versioned_object_update_spec(name: u8, value: u8, version: u8) WorkflowScheduleSpec {
    id := workflow_object_workflow_id(name)
    return workflow_schedule_spec(id, version, workflow_payload_object_update(value))
}

func workflow_schedule_update_apply_spec() WorkflowScheduleSpec {
    return workflow_schedule_spec(WORKFLOW_UPDATE_APPLY_ID, 0, workflow_payload_terminal())
}

func workflow_schedule_update_apply_with_lease_spec(id: u8) WorkflowScheduleSpec {
    return workflow_schedule_spec(WORKFLOW_UPDATE_APPLY_ID, id, workflow_payload_terminal())
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

func workflow_is_update_apply(s: WorkflowServiceState) bool {
    if !workflow_has_record(s) {
        return false
    }
    return s.kind == WORKFLOW_KIND_UPDATE_APPLY
}

func workflow_has_record(s: WorkflowServiceState) bool {
    return !workflow_is_idle(s)
}

func workflow_object_workflow_id(name: u8) u8 {
    return name + 1
}

func workflow_connection_slot(s: WorkflowServiceState) u8 {
    if s.due < WORKFLOW_CONNECTION_SLOT_MARKER {
        return s.opcode
    }
    return s.due - WORKFLOW_CONNECTION_SLOT_MARKER
}

func workflow_connection_uses_ticket_lease(s: WorkflowServiceState) bool {
    return s.opcode >= connection_service.CONNECTION_EXTERNAL_TICKET_BASE
}

func workflow_connection_ticket_lease_id(s: WorkflowServiceState) u8 {
    if !workflow_connection_uses_ticket_lease(s) {
        return 0
    }
    return s.opcode - connection_service.CONNECTION_EXTERNAL_TICKET_BASE
}

func workflow_object_name(s: WorkflowServiceState) u8 {
    return s.id - 1
}

func workflow_object_expected_version(s: WorkflowServiceState) u8 {
    return s.opcode
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

func workflow_schedule_versioned_object_update(s: WorkflowServiceState, name: u8, value: u8, version: u8) WorkflowResult {
    if version == 0 {
        return WorkflowResult{ state: s, effect: workflow_reply_invalid() }
    }
    if name == 255 {
        return WorkflowResult{ state: s, effect: workflow_reply_invalid() }
    }
    if !workflow_can_schedule(s) {
        return WorkflowResult{ state: s, effect: workflow_reply_invalid() }
    }
    spec := workflow_schedule_versioned_object_update_spec(name, value, version)
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

func workflow_schedule_update_apply(s: WorkflowServiceState) WorkflowResult {
    if !workflow_can_schedule(s) {
        return WorkflowResult{ state: s, effect: workflow_reply_invalid() }
    }
    spec := workflow_schedule_update_apply_spec()
    next := workflow_record(s, WORKFLOW_KIND_UPDATE_APPLY, spec.id, spec.opcode, spec.payload, WORKFLOW_STATE_WAITING, WORKFLOW_RESTART_NONE, s.generation)
    return WorkflowResult{ state: next, effect: workflow_schedule_effect(spec.id, WORKFLOW_STATE_WAITING) }
}

func workflow_schedule_update_apply_with_lease(s: WorkflowServiceState, lease: u8) WorkflowResult {
    if lease == 0 {
        return WorkflowResult{ state: s, effect: workflow_reply_invalid() }
    }
    if !workflow_can_schedule(s) {
        return WorkflowResult{ state: s, effect: workflow_reply_invalid() }
    }
    spec := workflow_schedule_update_apply_with_lease_spec(lease)
    next := workflow_record(s, WORKFLOW_KIND_UPDATE_APPLY, spec.id, spec.opcode, spec.payload, WORKFLOW_STATE_WAITING, WORKFLOW_RESTART_NONE, s.generation)
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

func workflow_waiting_task(s: WorkflowServiceState, due: u8, restart: u8, generation: u8) WorkflowServiceState {
    return workflow_restate(s, workflow_payload_wait(due), WORKFLOW_STATE_WAITING, restart, generation)
}

func workflow_waiting_object_update(s: WorkflowServiceState, restart: u8, generation: u8) WorkflowServiceState {
    return workflow_restate(s, workflow_payload_object_update(workflow_object_value(s)), WORKFLOW_STATE_WAITING, restart, generation)
}

func workflow_waiting_update_apply(s: WorkflowServiceState, restart: u8, generation: u8) WorkflowServiceState {
    return workflow_restate(s, workflow_payload_terminal(), WORKFLOW_STATE_WAITING, restart, generation)
}

func workflow_update_apply_lease_id(s: WorkflowServiceState) u8 {
    return s.opcode
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
    if workflow_is_connection(s) {
        return workflow_state_raw(s, s.kind, s.id, status, workflow_connection_slot(s), outcome, WORKFLOW_STATE_DELIVERING, s.restart, s.generation)
    }
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
    if m.payload[0] == WORKFLOW_OP_APPLY_UPDATE || m.payload[0] == WORKFLOW_OP_APPLY_UPDATE_LEASE {
        return WORKFLOW_UPDATE_APPLY_DELAY
    }
    return m.payload[2]
}

func workflow_restart_timer_due(s: WorkflowServiceState) u8 {
    if workflow_is_object_update(s) {
        return WORKFLOW_OBJECT_UPDATE_DELAY
    }
    if workflow_is_update_apply(s) {
        return WORKFLOW_UPDATE_APPLY_DELAY
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
    if workflow_is_update_apply(s) {
        return WORKFLOW_STATE_UPDATE_CANCELLED
    }
    return WORKFLOW_STATE_CANCELLED
}

func workflow_object_update_outcome(effect: service_effect.Effect) u8 {
    if service_effect.effect_reply_status(effect) == syscall.SyscallStatus.Ok {
        return WORKFLOW_STATE_OBJECT_UPDATED
    }
    if service_effect.effect_reply_payload_len(effect) == 1 && service_effect.effect_reply_payload(effect)[0] == object_store_service.OBJECT_UPDATE_CONFLICT {
        return WORKFLOW_STATE_OBJECT_CONFLICT
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

func workflow_connection_ticket_outcome(effect: service_effect.Effect) u8 {
    if service_effect.effect_reply_status(effect) == syscall.SyscallStatus.Ok {
        return WORKFLOW_STATE_CONNECTION_TICKET_USED
    }
    if service_effect.effect_reply_payload_len(effect) == 0 {
        return WORKFLOW_STATE_CONNECTION_TICKET_INVALID
    }

    code := service_effect.effect_reply_payload(effect)[0]
    if code == lease_service.LEASE_STALE || code == ticket_service.TICKET_STALE {
        return WORKFLOW_STATE_CONNECTION_TICKET_STALE
    }
    if code == ticket_service.TICKET_CONSUMED {
        return WORKFLOW_STATE_CONNECTION_TICKET_CONSUMED
    }
    return WORKFLOW_STATE_CONNECTION_TICKET_INVALID
}

func workflow_update_apply_lease_outcome(effect: service_effect.Effect) u8 {
    if service_effect.effect_reply_payload_len(effect) == 0 {
        return WORKFLOW_STATE_UPDATE_LEASE_INVALID
    }

    code := service_effect.effect_reply_payload(effect)[0]
    if code == lease_service.LEASE_STALE {
        return WORKFLOW_STATE_UPDATE_LEASE_STALE
    }
    if code == lease_service.LEASE_CONSUMED {
        return WORKFLOW_STATE_UPDATE_LEASE_CONSUMED
    }
    return WORKFLOW_STATE_UPDATE_LEASE_INVALID
}

func workflow_active_count(s: WorkflowServiceState) usize {
    if workflow_is_active(s) {
        return 1
    }
    return 0
}