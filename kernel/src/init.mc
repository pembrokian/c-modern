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
import echo_service
import kv_service
import log_service
import queue_service
import service_topology
import ticket_service
import transfer_service

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
    default:
        return state
    }
}

func restart_log(state: boot.KernelBootState) boot.KernelBootState {
    log_slot: service_topology.ServiceSlot = service_topology.LOG_SLOT
    snap: log_service.LogSnapshot = log_service.log_snapshot(state.log.state)
    next: boot.KernelBootState = boot.bootrestart_log(state, log_service.log_reload(log_slot.pid, 1, snap))
    return boot.bootwith_log_restart_outcome(next, boot.RestartOutcome.RetainedReloaded)
}

func restart_kv(state: boot.KernelBootState) boot.KernelBootState {
    kv_slot: service_topology.ServiceSlot = service_topology.KV_SLOT
    snap: kv_service.KvSnapshot = kv_service.kv_snapshot(state.kv.state)
    next: boot.KernelBootState = boot.bootrestart_kv(state, kv_service.kv_reload(kv_slot.pid, 1, snap))
    return boot.bootwith_kv_restart_outcome(next, boot.RestartOutcome.RetainedReloaded)
}

func restart_queue(state: boot.KernelBootState) boot.KernelBootState {
    queue_slot: service_topology.ServiceSlot = service_topology.QUEUE_SLOT
    snap: queue_service.QueueSnapshot = queue_service.queue_snapshot(state.queue.state)
    next: boot.KernelBootState = boot.bootrestart_queue(state, queue_service.queue_reload(queue_slot.pid, 1, snap))
    return boot.bootwith_queue_restart_outcome(next, boot.RestartOutcome.RetainedReloaded)
}

func restart_retained_workset(state: boot.KernelBootState) boot.KernelBootState {
    kv_slot: service_topology.ServiceSlot = service_topology.KV_SLOT
    queue_slot: service_topology.ServiceSlot = service_topology.QUEUE_SLOT
    kv_snap: kv_service.KvSnapshot = kv_service.kv_snapshot(state.kv.state)
    queue_snap: queue_service.QueueSnapshot = queue_service.queue_snapshot(state.queue.state)

    next: boot.KernelBootState = boot.bootrestart_kv(state, kv_service.kv_reload(kv_slot.pid, 1, kv_snap))
    next = boot.bootrestart_queue(next, queue_service.queue_reload(queue_slot.pid, 1, queue_snap))
    next = boot.bootrestart_workset_generation(next)
    next = boot.bootwith_kv_restart_outcome(next, boot.RestartOutcome.CoordinatedRetainedReloaded)
    next = boot.bootwith_queue_restart_outcome(next, boot.RestartOutcome.CoordinatedRetainedReloaded)
    return boot.bootwith_workset_restart_outcome(next, boot.RestartOutcome.CoordinatedRetainedReloaded)
}

func restart_retained_audit_lane(state: boot.KernelBootState) boot.KernelBootState {
    kv_slot: service_topology.ServiceSlot = service_topology.KV_SLOT
    log_slot: service_topology.ServiceSlot = service_topology.LOG_SLOT
    kv_snap: kv_service.KvSnapshot = kv_service.kv_snapshot(state.kv.state)
    log_snap: log_service.LogSnapshot = log_service.log_snapshot(state.log.state)

    next: boot.KernelBootState = boot.bootrestart_kv(state, kv_service.kv_reload(kv_slot.pid, 1, kv_snap))
    next = boot.bootrestart_log(next, log_service.log_reload(log_slot.pid, 1, log_snap))
    next = boot.bootrestart_audit_generation(next)
    next = boot.bootwith_kv_restart_outcome(next, boot.RestartOutcome.CoordinatedRetainedReloaded)
    next = boot.bootwith_log_restart_outcome(next, boot.RestartOutcome.CoordinatedRetainedReloaded)
    return boot.bootwith_audit_restart_outcome(next, boot.RestartOutcome.CoordinatedRetainedReloaded)
}

func restart_echo(state: boot.KernelBootState) boot.KernelBootState {
    echo_slot: service_topology.ServiceSlot = service_topology.ECHO_SLOT
    next: boot.KernelBootState = boot.bootrestart_echo(state, echo_service.echo_init(echo_slot.pid, 1))
    return boot.bootwith_echo_restart_outcome(next, boot.RestartOutcome.OrdinaryReplaced)
}

func restart_transfer(state: boot.KernelBootState) boot.KernelBootState {
    transfer_slot: service_topology.ServiceSlot = service_topology.TRANSFER_SLOT
    next: boot.KernelBootState = boot.bootrestart_transfer(state, transfer_service.transfer_init(transfer_slot.pid, 1))
    return boot.bootwith_transfer_restart_outcome(next, boot.RestartOutcome.OrdinaryReplaced)
}

func restart_ticket(state: boot.KernelBootState) boot.KernelBootState {
    ticket_slot: service_topology.ServiceSlot = service_topology.TICKET_SLOT
    next: boot.KernelBootState = boot.bootrestart_ticket(state, ticket_service.ticket_init(ticket_slot.pid, 1, state.ticket.state.epoch + 1))
    return boot.bootwith_ticket_restart_outcome(next, boot.RestartOutcome.OrdinaryReplaced)
}
