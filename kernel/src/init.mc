// Restart policy for exhausted boot services.
//
// This module owns the decision to replace a service's retained state when
// the caller observes an Exhausted reply.  It is not an IPC service and
// has no endpoint.  Mechanism (bootwith_*) lives in boot.mc; policy lives
// here so init is the named owner of the restart contract.
//
// Stability rules from service_identity.mc apply: the endpoint_id survives
// restart, retained state does not.  Callers that held a ServiceRef before
// restart may resume sending without reacquiring the ref.

import boot
import echo_service
import kv_service
import log_service
import service_topology

func can_restart(endpoint: u32) bool {
    if endpoint == service_topology.LOG_ENDPOINT_ID {
        return true
    }
    if endpoint == service_topology.KV_ENDPOINT_ID {
        return true
    }
    if endpoint == service_topology.ECHO_ENDPOINT_ID {
        return true
    }
    return false
}

func restart(state: boot.KernelBootState, endpoint: u32) boot.KernelBootState {
    if endpoint == service_topology.LOG_ENDPOINT_ID {
        return restart_log(state)
    }
    if endpoint == service_topology.KV_ENDPOINT_ID {
        return restart_kv(state)
    }
    if endpoint == service_topology.ECHO_ENDPOINT_ID {
        return restart_echo(state)
    }
    return state
}

func restart_log(state: boot.KernelBootState) boot.KernelBootState {
    log_slot: service_topology.ServiceSlot = service_topology.LOG_SLOT
    return boot.bootwith_log(state, log_service.log_init(log_slot.pid, 1))
}

func restart_kv(state: boot.KernelBootState) boot.KernelBootState {
    kv_slot: service_topology.ServiceSlot = service_topology.KV_SLOT
    return boot.bootwith_kv(state, kv_service.kv_init(kv_slot.pid, 1))
}

func restart_echo(state: boot.KernelBootState) boot.KernelBootState {
    echo_slot: service_topology.ServiceSlot = service_topology.ECHO_SLOT
    return boot.bootwith_echo(state, echo_service.echo_init(echo_slot.pid, 1))
}
