// Restart policy for exhausted boot services.
//
// This module owns the decision to replace a service's retained state when
// the caller observes an Exhausted reply.  It is not an IPC service and
// has no endpoint.  Mechanism (bootwith_*) lives in boot.mc; policy lives
// here so init is the named owner of the restart contract.
//
// Stability rules from service_identity.mc apply: the endpoint_id survives
// restart, and retained state survives only when init chooses an explicit
// save-and-reload path.  Callers that held a ServiceRef before restart may
// resume sending without reacquiring the ref.  Restart does not preserve
// service-instance identity: init replaces the instance and bumps the
// generation marker in boot state even when the endpoint_id and pid stay
// fixed.

import boot
import completion_mailbox_service
import echo_service
import file_service
import journal_service
import kv_service
import lease_service
import log_service
import object_store_service
import queue_service
import serial_protocol
import service_topology
import task_service
import ticket_service
import timer_service
import transfer_service
import workflow_service

struct RestartPolicyInfo {
    target: u8
    owner0: u8
    owner1: u8
    policy: u8
}

func restart_policy_single(target: u8, policy: u8) RestartPolicyInfo {
    return RestartPolicyInfo{ target: target, owner0: target, owner1: serial_protocol.PARTICIPANT_NONE, policy: policy }
}

func restart_policy_pair(target: u8, owner0: u8, owner1: u8, policy: u8) RestartPolicyInfo {
    return RestartPolicyInfo{ target: target, owner0: owner0, owner1: owner1, policy: policy }
}

func restart_policy_for_target(target: u8) RestartPolicyInfo {
    switch target {
    case serial_protocol.TARGET_WORKSET:
        return restart_policy_pair(target, serial_protocol.TARGET_KV, serial_protocol.TARGET_QUEUE, serial_protocol.POLICY_KEEP)
    case serial_protocol.TARGET_AUDIT:
        return restart_policy_pair(target, serial_protocol.TARGET_KV, serial_protocol.TARGET_LOG, serial_protocol.POLICY_KEEP)
    case serial_protocol.TARGET_LOG:
        return restart_policy_single(target, serial_protocol.POLICY_KEEP)
    case serial_protocol.TARGET_KV:
        return restart_policy_single(target, serial_protocol.POLICY_KEEP)
    case serial_protocol.TARGET_QUEUE:
        return restart_policy_single(target, serial_protocol.POLICY_KEEP)
    case serial_protocol.TARGET_ECHO:
        return restart_policy_single(target, serial_protocol.POLICY_CLEAR)
    case serial_protocol.TARGET_TRANSFER:
        return restart_policy_single(target, serial_protocol.POLICY_CLEAR)
    case serial_protocol.TARGET_TICKET:
        return restart_policy_single(target, serial_protocol.POLICY_CLEAR)
    case serial_protocol.TARGET_FILE:
        return restart_policy_single(target, serial_protocol.POLICY_KEEP)
    case serial_protocol.TARGET_TIMER:
        return restart_policy_single(target, serial_protocol.POLICY_KEEP)
    case serial_protocol.TARGET_TASK:
        return restart_policy_single(target, serial_protocol.POLICY_CLEAR)
    case serial_protocol.TARGET_JOURNAL:
        return restart_policy_single(target, serial_protocol.POLICY_KEEP)
    case serial_protocol.TARGET_WORKFLOW:
        return restart_policy_single(target, serial_protocol.POLICY_KEEP)
    case serial_protocol.TARGET_LEASE:
        return restart_policy_single(target, serial_protocol.POLICY_CLEAR)
    case serial_protocol.TARGET_COMPLETION:
        return restart_policy_single(target, serial_protocol.POLICY_KEEP)
    case serial_protocol.TARGET_OBJECT_STORE:
        return restart_policy_single(target, serial_protocol.POLICY_KEEP)
    default:
        return RestartPolicyInfo{ target: 0, owner0: serial_protocol.PARTICIPANT_NONE, owner1: serial_protocol.PARTICIPANT_NONE, policy: serial_protocol.PARTICIPANT_NONE }
    }
}

func restart_policy_payload(target: u8) [4]u8 {
    info := restart_policy_for_target(target)
    payload: [4]u8
    payload[0] = info.target
    payload[1] = info.owner0
    payload[2] = info.owner1
    payload[3] = info.policy
    return payload
}

func restart(state: boot.KernelBootState, endpoint: u32) boot.KernelBootState {
    if !service_topology.service_can_restart(endpoint) {
        return state
    }
    switch endpoint {
    case service_topology.LOG_ENDPOINT_ID:
        return restart_log(state)
    case service_topology.KV_ENDPOINT_ID:
        return restart_kv(state)
    case service_topology.QUEUE_ENDPOINT_ID:
        return restart_queue(state)
    case service_topology.ECHO_ENDPOINT_ID:
        return restart_echo(state)
    case service_topology.TRANSFER_ENDPOINT_ID:
        return restart_transfer(state)
    case service_topology.TICKET_ENDPOINT_ID:
        return restart_ticket(state)
    case service_topology.FILE_ENDPOINT_ID:
        return restart_file(state)
    case service_topology.TIMER_ENDPOINT_ID:
        return restart_timer(state)
    case service_topology.TASK_ENDPOINT_ID:
        return restart_task(state)
    case service_topology.JOURNAL_ENDPOINT_ID:
        return restart_journal(state)
    case service_topology.WORKFLOW_ENDPOINT_ID:
        return restart_workflow(state)
    case service_topology.LEASE_ENDPOINT_ID:
        return restart_lease(state)
    case service_topology.COMPLETION_MAILBOX_ENDPOINT_ID:
        return restart_completion_mailbox(state)
    case service_topology.OBJECT_STORE_ENDPOINT_ID:
        return restart_object_store(state)
    default:
        return state
    }
}

func restart_log(state: boot.KernelBootState) boot.KernelBootState {
    log_slot := service_topology.LOG_SLOT
    snap := log_service.log_snapshot(state.log.state)
    next := boot.bootrestart_log(state, log_service.log_reload(log_slot.pid, 1, snap))
    return boot.bootwith_log_restart_outcome(next, boot.RestartOutcome.RetainedReloaded)
}

func restart_kv(state: boot.KernelBootState) boot.KernelBootState {
    kv_slot := service_topology.KV_SLOT
    snap := kv_service.kv_snapshot(state.kv.state)
    next := boot.bootrestart_kv(state, kv_service.kv_reload(kv_slot.pid, 1, snap))
    return boot.bootwith_kv_restart_outcome(next, boot.RestartOutcome.RetainedReloaded)
}

func restart_queue(state: boot.KernelBootState) boot.KernelBootState {
    queue_slot := service_topology.QUEUE_SLOT
    snap := queue_service.queue_snapshot(state.queue.state)
    next := boot.bootrestart_queue(state, queue_service.queue_reload(queue_slot.pid, 1, snap))
    return boot.bootwith_queue_restart_outcome(next, boot.RestartOutcome.RetainedReloaded)
}

func restart_retained_workset(state: boot.KernelBootState) boot.KernelBootState {
    kv_slot := service_topology.KV_SLOT
    queue_slot := service_topology.QUEUE_SLOT
    kv_snap := kv_service.kv_snapshot(state.kv.state)
    queue_snap := queue_service.queue_snapshot(state.queue.state)

    next := boot.bootrestart_kv(state, kv_service.kv_reload(kv_slot.pid, 1, kv_snap))
    next = boot.bootrestart_queue(next, queue_service.queue_reload(queue_slot.pid, 1, queue_snap))
    next = boot.bootrestart_workset_generation(next)
    next = boot.bootwith_kv_restart_outcome(next, boot.RestartOutcome.CoordinatedRetainedReloaded)
    next = boot.bootwith_queue_restart_outcome(next, boot.RestartOutcome.CoordinatedRetainedReloaded)
    return boot.bootwith_workset_restart_outcome(next, boot.RestartOutcome.CoordinatedRetainedReloaded)
}

func restart_retained_audit_lane(state: boot.KernelBootState) boot.KernelBootState {
    kv_slot := service_topology.KV_SLOT
    log_slot := service_topology.LOG_SLOT
    kv_snap := kv_service.kv_snapshot(state.kv.state)
    log_snap := log_service.log_snapshot(state.log.state)

    next := boot.bootrestart_kv(state, kv_service.kv_reload(kv_slot.pid, 1, kv_snap))
    next = boot.bootrestart_log(next, log_service.log_reload(log_slot.pid, 1, log_snap))
    next = boot.bootrestart_audit_generation(next)
    next = boot.bootwith_kv_restart_outcome(next, boot.RestartOutcome.CoordinatedRetainedReloaded)
    next = boot.bootwith_log_restart_outcome(next, boot.RestartOutcome.CoordinatedRetainedReloaded)
    return boot.bootwith_audit_restart_outcome(next, boot.RestartOutcome.CoordinatedRetainedReloaded)
}

func restart_echo(state: boot.KernelBootState) boot.KernelBootState {
    echo_slot := service_topology.ECHO_SLOT
    next := boot.bootrestart_echo(state, echo_service.echo_init(echo_slot.pid, 1))
    return boot.bootwith_echo_restart_outcome(next, boot.RestartOutcome.OrdinaryReplaced)
}

func restart_transfer(state: boot.KernelBootState) boot.KernelBootState {
    transfer_slot := service_topology.TRANSFER_SLOT
    next := boot.bootrestart_transfer(state, transfer_service.transfer_init(transfer_slot.pid, 1))
    return boot.bootwith_transfer_restart_outcome(next, boot.RestartOutcome.OrdinaryReplaced)
}

func restart_ticket(state: boot.KernelBootState) boot.KernelBootState {
    ticket_slot := service_topology.TICKET_SLOT
    next := boot.bootrestart_ticket(state, ticket_service.ticket_init(ticket_slot.pid, 1, state.ticket.state.epoch + 1))
    return boot.bootwith_ticket_restart_outcome(next, boot.RestartOutcome.OrdinaryReplaced)
}

func restart_file(state: boot.KernelBootState) boot.KernelBootState {
    file_slot := service_topology.FILE_SLOT
    snap_slot0: file_service.FileSlot = state.file.state.slot0
    snap_slot1: file_service.FileSlot = state.file.state.slot1
    snap_slot2: file_service.FileSlot = state.file.state.slot2
    snap_slot3: file_service.FileSlot = state.file.state.slot3
    snap_count: usize = state.file.state.count
    reloaded: file_service.FileServiceState = file_service.FileServiceState{ pid: file_slot.pid, slot: 1, slot0: snap_slot0, slot1: snap_slot1, slot2: snap_slot2, slot3: snap_slot3, count: snap_count }
    next := boot.bootrestart_file(state, reloaded)
    return boot.bootwith_file_restart_outcome(next, boot.RestartOutcome.RetainedReloaded)
}

func restart_timer(state: boot.KernelBootState) boot.KernelBootState {
    timer_slot := service_topology.TIMER_SLOT
    snap := timer_service.timer_snapshot(state.timer.state)
    next := boot.bootrestart_timer(state, timer_service.timer_reload(timer_slot.pid, 1, snap))
    return boot.bootwith_timer_restart_outcome(next, boot.RestartOutcome.RetainedReloaded)
}

func restart_task(state: boot.KernelBootState) boot.KernelBootState {
    task_slot := service_topology.TASK_SLOT
    next := boot.bootrestart_task(state, task_service.task_init(task_slot.pid, 1))
    return boot.bootwith_task_restart_outcome(next, boot.RestartOutcome.OrdinaryReplaced)
}

func restart_journal(state: boot.KernelBootState) boot.KernelBootState {
    journal_slot := service_topology.JOURNAL_SLOT
    next := boot.bootrestart_journal(state, journal_service.journal_load(journal_slot.pid, 1))
    return boot.bootwith_journal_restart_outcome(next, boot.RestartOutcome.DurableReloaded)
}

func restart_workflow(state: boot.KernelBootState) boot.KernelBootState {
    workflow_slot := service_topology.WORKFLOW_SLOT
    snap := workflow_service.workflow_snapshot_record(state.workflow.state)
    generation: u8 = u8(state.workset_generation)
    reloaded := workflow_service.workflow_restart_reload(workflow_slot.pid, snap, state.journal.state.lane1, generation, true)
    next := state
    next_timer := state.timer.state

    if reloaded.state == workflow_service.WORKFLOW_STATE_WAITING {
        timer_due := workflow_service.workflow_restart_timer_due(reloaded)
        timer_create_result := timer_service.timer_create(next_timer, workflow_service.WORKFLOW_TIMER_ID, timer_due)
        next_timer = timer_create_result.state
    } else {
        timer_cancel_result := timer_service.timer_cancel(next_timer, workflow_service.WORKFLOW_TIMER_ID)
        next_timer = timer_cancel_result.state
    }

    next = boot.bootwith_timer(next, next_timer)
    return boot.bootrestart_workflow(next, reloaded)
}

func restart_lease(state: boot.KernelBootState) boot.KernelBootState {
    slot := service_topology.LEASE_SLOT
    next := boot.bootrestart_lease(state, lease_service.lease_init(slot.pid, 1))
    return boot.bootwith_lease_restart_outcome(next, boot.RestartOutcome.OrdinaryReplaced)
}

func restart_completion_mailbox(state: boot.KernelBootState) boot.KernelBootState {
    slot := service_topology.COMPLETION_MAILBOX_SLOT
    snap := completion_mailbox_service.completion_mailbox_snapshot(state.completion.state)
    next := boot.bootrestart_completion(state, completion_mailbox_service.completion_mailbox_reload(slot.pid, 1, snap))
    return boot.bootwith_completion_restart_outcome(next, boot.RestartOutcome.RetainedReloaded)
}

func restart_object_store(state: boot.KernelBootState) boot.KernelBootState {
    slot := service_topology.OBJECT_STORE_SLOT
    next := boot.bootrestart_object_store(state, object_store_service.object_store_load(slot.pid, 1))
    return boot.bootwith_object_store_restart_outcome(next, boot.RestartOutcome.DurableReloaded)
}
