import identity_taxonomy
import service_identity
import serial_protocol
import service_topology

// Named ServiceRef accessors for the four boot-wired services.
// These refs are stable across restart -- the endpoint_id never changes after
// kernel_init() assigns it. Callers that hold one of these refs may resume
// sending after a service restart without reacquiring the ref.

func boot_serial_ref() service_identity.ServiceRef {
    return service_identity.service_ref(service_topology.SERIAL_ENDPOINT_ID)
}

func boot_shell_ref() service_identity.ServiceRef {
    return service_identity.service_ref(service_topology.SHELL_ENDPOINT_ID)
}

func boot_log_ref() service_identity.ServiceRef {
    return service_identity.service_ref(service_topology.LOG_ENDPOINT_ID)
}

func boot_kv_ref() service_identity.ServiceRef {
    return service_identity.service_ref(service_topology.KV_ENDPOINT_ID)
}

func boot_echo_ref() service_identity.ServiceRef {
    return service_identity.service_ref(service_topology.ECHO_ENDPOINT_ID)
}

func boot_queue_ref() service_identity.ServiceRef {
    return service_identity.service_ref(service_topology.QUEUE_ENDPOINT_ID)
}

func boot_transfer_ref() service_identity.ServiceRef {
    return service_identity.service_ref(service_topology.TRANSFER_ENDPOINT_ID)
}

func boot_ticket_ref() service_identity.ServiceRef {
    return service_identity.service_ref(service_topology.TICKET_ENDPOINT_ID)
}

func boot_log_mark(s: KernelBootState) service_identity.ServiceMark {
    return service_identity.service_mark(service_topology.LOG_ENDPOINT_ID, s.log.state.pid, s.log.generation)
}

func boot_kv_mark(s: KernelBootState) service_identity.ServiceMark {
    return service_identity.service_mark(service_topology.KV_ENDPOINT_ID, s.kv.state.pid, s.kv.generation)
}

func boot_queue_mark(s: KernelBootState) service_identity.ServiceMark {
    return service_identity.service_mark(service_topology.QUEUE_ENDPOINT_ID, s.queue.state.pid, s.queue.generation)
}

func boot_workset_generation(s: KernelBootState) u32 {
    return s.workset_generation
}

func boot_audit_generation(s: KernelBootState) u32 {
    return s.audit_generation
}

func boot_workset_generation_payload(s: KernelBootState) [4]u8 {
    return service_identity.generation_payload(s.workset_generation)
}

func summary_participation_code(participation: RetainedSummaryParticipation) u8 {
    switch participation {
    case RetainedSummaryParticipation.Service:
        return 1
    case RetainedSummaryParticipation.Lane:
        return 2
    default:
        return 0
    }
}

func summary_outcome_code(outcome: RestartOutcome) u8 {
    switch outcome {
    case RestartOutcome.OrdinaryReplaced:
        return 1
    case RestartOutcome.RetainedReloaded:
        return 2
    case RestartOutcome.CoordinatedRetainedReloaded:
        return 3
    default:
        return 0
    }
}

func summary_payload(participation: RetainedSummaryParticipation, outcome: RestartOutcome, generation: u32) [4]u8 {
    payload: [4]u8
    payload[0] = u8((summary_participation_code(participation) << 4) | summary_outcome_code(outcome))
    payload[1] = u8(generation >> 16)
    payload[2] = u8(generation >> 8)
    payload[3] = u8(generation)
    return payload
}

func bootrestart_outcome_for_endpoint(s: KernelBootState, endpoint: u32) RestartOutcome {
    switch endpoint {
    case service_topology.LOG_ENDPOINT_ID:
        return s.log_restart_outcome
    case service_topology.KV_ENDPOINT_ID:
        return s.kv_restart_outcome
    case service_topology.QUEUE_ENDPOINT_ID:
        return s.queue_restart_outcome
    case service_topology.ECHO_ENDPOINT_ID:
        return s.echo_restart_outcome
    case service_topology.TRANSFER_ENDPOINT_ID:
        return s.transfer_restart_outcome
    case service_topology.TICKET_ENDPOINT_ID:
        return s.ticket_restart_outcome
    default:
        return RestartOutcome.None
    }
}

func bootsummary_payload_for_endpoint(s: KernelBootState, endpoint: u32) [4]u8 {
    participation: RetainedSummaryParticipation = RetainedSummaryParticipation.None
    if endpoint == service_topology.LOG_ENDPOINT_ID || endpoint == service_topology.KV_ENDPOINT_ID || endpoint == service_topology.QUEUE_ENDPOINT_ID {
        participation = RetainedSummaryParticipation.Service
    }
    mark: service_identity.ServiceMark = bootmark_for_endpoint(s, endpoint)
    return summary_payload(participation, bootrestart_outcome_for_endpoint(s, endpoint), service_identity.mark_generation(mark))
}

func bootsummary_payload_for_target(s: KernelBootState, target: u8) [4]u8 {
    if identity_taxonomy.identity_target_is_lane(target) {
        if target == serial_protocol.TARGET_WORKSET {
            return summary_payload(RetainedSummaryParticipation.Lane, s.workset_restart_outcome, s.workset_generation)
        }
        return summary_payload(RetainedSummaryParticipation.Lane, s.audit_restart_outcome, s.audit_generation)
    }
    endpoint: u32 = shell_service.lifecycle_target_endpoint(target)
    return bootsummary_payload_for_endpoint(s, endpoint)
}

func boot_echo_mark(s: KernelBootState) service_identity.ServiceMark {
    return service_identity.service_mark(service_topology.ECHO_ENDPOINT_ID, s.echo.state.pid, s.echo.generation)
}

func boot_transfer_mark(s: KernelBootState) service_identity.ServiceMark {
    return service_identity.service_mark(service_topology.TRANSFER_ENDPOINT_ID, s.transfer.state.pid, s.transfer.generation)
}

func boot_ticket_mark(s: KernelBootState) service_identity.ServiceMark {
    return service_identity.service_mark(service_topology.TICKET_ENDPOINT_ID, s.ticket.state.pid, s.ticket.generation)
}

func bootmark_for_endpoint(s: KernelBootState, endpoint: u32) service_identity.ServiceMark {
    switch endpoint {
    case service_topology.LOG_ENDPOINT_ID:
        return boot_log_mark(s)
    case service_topology.KV_ENDPOINT_ID:
        return boot_kv_mark(s)
    case service_topology.QUEUE_ENDPOINT_ID:
        return boot_queue_mark(s)
    case service_topology.ECHO_ENDPOINT_ID:
        return boot_echo_mark(s)
    case service_topology.TRANSFER_ENDPOINT_ID:
        return boot_transfer_mark(s)
    case service_topology.TICKET_ENDPOINT_ID:
        return boot_ticket_mark(s)
    default:
        return service_identity.service_mark(endpoint, 0, 0)
    }
}