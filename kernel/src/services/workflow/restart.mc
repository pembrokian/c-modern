import completion_mailbox_service
import connection_service
import journal_service
import lease_service
import object_store_service
import service_effect
import task_service
import ticket_service
import timer_service
import update_store_service
import workflow/core

func workflow_sync_journal(journal: journal_service.JournalServiceState, s: workflow_core.WorkflowServiceState) journal_service.JournalServiceState {
    lane := journal_service.lane_init(journal_service.JOURNAL_LANE_B)
    if workflow_core.workflow_has_record(s) || s.restart != workflow_core.WORKFLOW_RESTART_NONE {
        lane = journal_service.JournalLane{ name: journal_service.JOURNAL_LANE_B, len: workflow_core.WORKFLOW_JOURNAL_LANE_LEN, data: workflow_core.workflow_lane_bytes(s) }
    }
    return journal_service.journalwith(journal, journal.lane0, lane)
}

func workflow_step_with_synced_journal(workflow: workflow_core.WorkflowServiceState, step: workflow_core.WorkflowStepContext, effect: service_effect.Effect) workflow_core.WorkflowStepResult {
    synced := step with { journal: workflow_sync_journal(step.journal, workflow) }
    return workflow_core.workflow_step_result(workflow, synced.runtime.timer, synced.runtime.task, synced.runtime.object_store, synced.runtime.update_store, synced.journal, synced.runtime.lease, synced.runtime.ticket, synced.runtime.completion, effect)
}

func workflow_lane_matches(s: workflow_core.WorkflowServiceState, lane: journal_service.JournalLane) bool {
    if lane.name != journal_service.JOURNAL_LANE_B || lane.len != workflow_core.WORKFLOW_JOURNAL_LANE_LEN {
        return false
    }
    bytes := workflow_core.workflow_lane_bytes(s)
    for i in 0..workflow_core.WORKFLOW_JOURNAL_LANE_LEN {
        if lane.data[i] != bytes[i] {
            return false
        }
    }
    return true
}

func workflow_restart_reload(pid: u32, snap: workflow_core.WorkflowSnapshotRecord, lane: journal_service.JournalLane, generation: u8, keep: bool, connection: connection_service.ConnectionServiceState) workflow_core.WorkflowServiceState {
    current := workflow_core.workflow_state_from_snapshot(pid, snap)
    if !workflow_core.workflow_has_record(current) {
        return current
    }
    if workflow_core.workflow_is_delivering(current) {
        delivery := workflow_core.workflow_delivery_state(workflow_core.workflow_delivery_outcome(current), workflow_core.workflow_delivery_status(current))
        if !keep {
            return workflow_core.workflow_restate(current, workflow_core.workflow_payload_delivery(delivery.outcome, delivery.status), current.state, workflow_core.WORKFLOW_RESTART_CANCELLED, generation)
        }
        return workflow_core.workflow_restate(current, workflow_core.workflow_payload_delivery(delivery.outcome, delivery.status), current.state, workflow_core.WORKFLOW_RESTART_RESUMED, generation)
    }
    if !workflow_core.workflow_is_active(current) {
        return workflow_core.workflow_restate(current, workflow_core.workflow_payload_from_state(current), current.state, workflow_core.WORKFLOW_RESTART_NONE, current.generation)
    }
    if !keep || current.generation != generation || !workflow_lane_matches(current, lane) {
        if workflow_core.workflow_is_connection(current) {
            return workflow_core.workflow_restate(current, workflow_core.workflow_payload_delivery(workflow_core.WORKFLOW_STATE_CONNECTION_RESTART_CANCELLED, 0), workflow_core.WORKFLOW_STATE_DELIVERING, workflow_core.WORKFLOW_RESTART_CANCELLED, generation)
        }
        if workflow_core.workflow_is_object_update(current) {
            return workflow_core.workflow_restate(current, workflow_core.workflow_payload_delivery(workflow_core.WORKFLOW_STATE_OBJECT_CANCELLED, 0), workflow_core.WORKFLOW_STATE_DELIVERING, workflow_core.WORKFLOW_RESTART_CANCELLED, generation)
        }
        if workflow_core.workflow_is_update_apply(current) {
            return workflow_core.workflow_restate(current, workflow_core.workflow_payload_delivery(workflow_core.WORKFLOW_STATE_UPDATE_CANCELLED, 0), workflow_core.WORKFLOW_STATE_DELIVERING, workflow_core.WORKFLOW_RESTART_CANCELLED, generation)
        }
        return workflow_core.workflow_restate(current, workflow_core.workflow_payload_terminal(), workflow_core.WORKFLOW_STATE_CANCELLED, workflow_core.WORKFLOW_RESTART_CANCELLED, generation)
    }
    if workflow_core.workflow_is_connection(current) && !workflow_core.workflow_connection_active(current, connection) {
        return workflow_core.workflow_restate(current, workflow_core.workflow_payload_delivery(workflow_core.WORKFLOW_STATE_CONNECTION_RESTART_CANCELLED, 0), workflow_core.WORKFLOW_STATE_DELIVERING, workflow_core.WORKFLOW_RESTART_CANCELLED, generation)
    }
    if workflow_core.workflow_is_running(current) {
        if workflow_core.workflow_is_connection(current) {
            return workflow_core.workflow_waiting_connection(current, workflow_core.workflow_connection_slot(current), workflow_core.WORKFLOW_RESTART_RESUMED, generation)
        }
        return workflow_core.workflow_waiting_task(current, 0, workflow_core.WORKFLOW_RESTART_RESUMED, generation)
    }
    if workflow_core.workflow_is_object_update(current) {
        return workflow_core.workflow_waiting_object_update(current, workflow_core.WORKFLOW_RESTART_RESUMED, generation)
    }
    if workflow_core.workflow_is_update_apply(current) {
        return workflow_core.workflow_waiting_update_apply(current, workflow_core.WORKFLOW_RESTART_RESUMED, generation)
    }
    if workflow_core.workflow_is_connection(current) {
        return workflow_core.workflow_waiting_connection(current, workflow_core.workflow_connection_slot(current), workflow_core.WORKFLOW_RESTART_RESUMED, generation)
    }
    return workflow_core.workflow_waiting_task(current, workflow_core.workflow_wait_due(current), workflow_core.WORKFLOW_RESTART_RESUMED, generation)
}