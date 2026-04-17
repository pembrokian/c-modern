import completion_mailbox_service
import connection_service
import identity_taxonomy
import file_service
import journal_service
import kv_service
import lease_service
import log_service
import object_store_service
import queue_service
import service_identity
import service_state
import serial_protocol
import service_topology
import shell_service
import task_service
import timer_service
import workflow_service

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

func bootstate_metadata_for_target(s: KernelBootState, target: u8) u8 {
    switch target {
    case serial_protocol.TARGET_LOG:
        return u8(log_service.log_len(s.log.state))
    case serial_protocol.TARGET_KV:
        return u8(kv_service.kv_count(s.kv.state))
    case serial_protocol.TARGET_QUEUE:
        return u8(queue_service.queue_len(s.queue.state))
    case serial_protocol.TARGET_FILE:
        return u8(file_service.file_count(s.file.state))
    case serial_protocol.TARGET_TIMER:
        return u8(timer_service.timer_active_count(s.timer.state))
    case serial_protocol.TARGET_TASK:
        return u8(task_service.task_active_count(s.task.state))
    case serial_protocol.TARGET_CONNECTION:
        return u8(connection_service.connection_active_count(s.connection.state))
    case serial_protocol.TARGET_JOURNAL:
        return u8(journal_service.journal_count(s.journal.state))
    case serial_protocol.TARGET_WORKFLOW:
        return u8(workflow_service.workflow_active_count(s.workflow.state))
    case serial_protocol.TARGET_LEASE:
        return u8(lease_service.lease_count(s.lease.state))
    case serial_protocol.TARGET_COMPLETION:
        return u8(completion_mailbox_service.completion_backlog_count(s.completion.state))
    case serial_protocol.TARGET_OBJECT_STORE:
        return u8(object_store_service.object_count(s.object_store.state))
    default:
        return 0
    }
}

func bootstate_generation_marker_for_target(s: KernelBootState, target: u8) u8 {
    if target == serial_protocol.TARGET_WORKSET {
        return service_state.state_generation_marker(s.workset_generation)
    }
    if target == serial_protocol.TARGET_AUDIT {
        return service_state.state_generation_marker(s.audit_generation)
    }

    endpoint: u32 = shell_service.lifecycle_target_endpoint(target)
    if endpoint == 0 || !service_topology.service_can_restart(endpoint) {
        return 0
    }

    mark: service_identity.ServiceMark = bootmark_for_endpoint(s, endpoint)
    return service_state.state_generation_marker(service_identity.mark_generation(mark))
}

func bootgeneration_payload_for_target(s: KernelBootState, target: u8) [4]u8 {
    if target == serial_protocol.TARGET_WORKSET {
        return boot_workset_generation_payload(s)
    }
    endpoint: u32 = shell_service.lifecycle_target_endpoint(target)
    if endpoint == 0 {
        return service_identity.generation_payload(0)
    }
    mark: service_identity.ServiceMark = bootmark_for_endpoint(s, endpoint)
    return service_identity.mark_generation_payload(mark)
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
    case RestartOutcome.DurableReloaded:
        return 4
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
    case service_topology.FILE_ENDPOINT_ID:
        return s.file_restart_outcome
    case service_topology.TIMER_ENDPOINT_ID:
        return s.timer_restart_outcome
    case service_topology.TASK_ENDPOINT_ID:
        return s.task_restart_outcome
    case service_topology.CONNECTION_ENDPOINT_ID:
        return s.connection_restart_outcome
    case service_topology.JOURNAL_ENDPOINT_ID:
        return s.journal_restart_outcome
    case service_topology.LEASE_ENDPOINT_ID:
        return s.lease_restart_outcome
    case service_topology.COMPLETION_MAILBOX_ENDPOINT_ID:
        return s.completion_restart_outcome
    case service_topology.OBJECT_STORE_ENDPOINT_ID:
        return s.object_store_restart_outcome
    default:
        return RestartOutcome.None
    }
}

func bootsummary_payload_for_endpoint(s: KernelBootState, endpoint: u32) [4]u8 {
    participation: RetainedSummaryParticipation = RetainedSummaryParticipation.None
    if endpoint == service_topology.LOG_ENDPOINT_ID || endpoint == service_topology.KV_ENDPOINT_ID || endpoint == service_topology.QUEUE_ENDPOINT_ID || endpoint == service_topology.FILE_ENDPOINT_ID || endpoint == service_topology.TIMER_ENDPOINT_ID || endpoint == service_topology.JOURNAL_ENDPOINT_ID || endpoint == service_topology.WORKFLOW_ENDPOINT_ID || endpoint == service_topology.LEASE_ENDPOINT_ID || endpoint == service_topology.COMPLETION_MAILBOX_ENDPOINT_ID || endpoint == service_topology.OBJECT_STORE_ENDPOINT_ID {
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

func bootcomparison_payload_for_target(s: KernelBootState, target: u8) [4]u8 {
    return bootsummary_payload_for_target(s, target)
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

func boot_file_mark(s: KernelBootState) service_identity.ServiceMark {
    return service_identity.service_mark(service_topology.FILE_ENDPOINT_ID, s.file.state.pid, s.file.generation)
}

func boot_timer_mark(s: KernelBootState) service_identity.ServiceMark {
    return service_identity.service_mark(service_topology.TIMER_ENDPOINT_ID, s.timer.state.pid, s.timer.generation)
}

func boot_task_mark(s: KernelBootState) service_identity.ServiceMark {
    return service_identity.service_mark(service_topology.TASK_ENDPOINT_ID, s.task.state.pid, s.task.generation)
}

func boot_connection_mark(s: KernelBootState) service_identity.ServiceMark {
    return service_identity.service_mark(service_topology.CONNECTION_ENDPOINT_ID, s.connection.state.pid, s.connection.generation)
}

func boot_journal_mark(s: KernelBootState) service_identity.ServiceMark {
    return service_identity.service_mark(service_topology.JOURNAL_ENDPOINT_ID, s.journal.state.pid, s.journal.generation)
}

func boot_workflow_mark(s: KernelBootState) service_identity.ServiceMark {
    return service_identity.service_mark(service_topology.WORKFLOW_ENDPOINT_ID, s.workflow.state.pid, s.workflow.generation)
}

func boot_lease_mark(s: KernelBootState) service_identity.ServiceMark {
    return service_identity.service_mark(service_topology.LEASE_ENDPOINT_ID, s.lease.state.pid, s.lease.generation)
}

func boot_completion_mark(s: KernelBootState) service_identity.ServiceMark {
    return service_identity.service_mark(service_topology.COMPLETION_MAILBOX_ENDPOINT_ID, s.completion.state.pid, s.completion.generation)
}

func boot_object_store_mark(s: KernelBootState) service_identity.ServiceMark {
    return service_identity.service_mark(service_topology.OBJECT_STORE_ENDPOINT_ID, s.object_store.state.pid, s.object_store.generation)
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
    case service_topology.FILE_ENDPOINT_ID:
        return boot_file_mark(s)
    case service_topology.TIMER_ENDPOINT_ID:
        return boot_timer_mark(s)
    case service_topology.TASK_ENDPOINT_ID:
        return boot_task_mark(s)
    case service_topology.CONNECTION_ENDPOINT_ID:
        return boot_connection_mark(s)
    case service_topology.JOURNAL_ENDPOINT_ID:
        return boot_journal_mark(s)
    case service_topology.WORKFLOW_ENDPOINT_ID:
        return boot_workflow_mark(s)
    case service_topology.LEASE_ENDPOINT_ID:
        return boot_lease_mark(s)
    case service_topology.COMPLETION_MAILBOX_ENDPOINT_ID:
        return boot_completion_mark(s)
    case service_topology.OBJECT_STORE_ENDPOINT_ID:
        return boot_object_store_mark(s)
    default:
        return service_identity.service_mark(endpoint, 0, 0)
    }
}