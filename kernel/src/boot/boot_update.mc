import completion_mailbox_service
import connection_service
import echo_service
import file_service
import journal_service
import kv_service
import lease_service
import log_service
import object_store_service
import queue_service
import serial_shell_path
import service_cell_helpers
import service_identity
import task_service
import ticket_service
import timer_service
import transfer_grant
import transfer_service
import update_store_service
import workflow_core

func bootwith_log(s: KernelBootState, log: log_service.LogServiceState) KernelBootState {
    return s with { log: service_cell_helpers.service_cell_with_state<log_service.LogServiceState>(s.log, log) }
}

func bootwith_path(s: KernelBootState, path_state: serial_shell_path.SerialShellPathState) KernelBootState {
    return s with { path_state: }
}

func bootwith_kv(s: KernelBootState, kv: kv_service.KvServiceState) KernelBootState {
    return s with { kv: service_cell_helpers.service_cell_with_state<kv_service.KvServiceState>(s.kv, kv) }
}

func bootwith_queue(s: KernelBootState, queue: queue_service.QueueServiceState) KernelBootState {
    return s with { queue: service_cell_helpers.service_cell_with_state<queue_service.QueueServiceState>(s.queue, queue) }
}

func bootwith_workset_generation(s: KernelBootState, workset_generation: u32) KernelBootState {
    return s with { workset_generation: }
}

func bootwith_audit_generation(s: KernelBootState, audit_generation: u32) KernelBootState {
    return s with { audit_generation: }
}

func bootwith_log_restart_outcome(s: KernelBootState, log_restart_outcome: RestartOutcome) KernelBootState {
    return s with { log_restart_outcome: }
}

func bootwith_kv_restart_outcome(s: KernelBootState, kv_restart_outcome: RestartOutcome) KernelBootState {
    return s with { kv_restart_outcome: }
}

func bootwith_queue_restart_outcome(s: KernelBootState, queue_restart_outcome: RestartOutcome) KernelBootState {
    return s with { queue_restart_outcome: }
}

func bootwith_workset_restart_outcome(s: KernelBootState, workset_restart_outcome: RestartOutcome) KernelBootState {
    return s with { workset_restart_outcome: }
}

func bootwith_audit_restart_outcome(s: KernelBootState, audit_restart_outcome: RestartOutcome) KernelBootState {
    return s with { audit_restart_outcome: }
}

func bootwith_echo(s: KernelBootState, echo: echo_service.EchoServiceState) KernelBootState {
    return s with { echo: service_cell_helpers.service_cell_with_state<echo_service.EchoServiceState>(s.echo, echo) }
}

func bootwith_echo_restart_outcome(s: KernelBootState, echo_restart_outcome: RestartOutcome) KernelBootState {
    return s with { echo_restart_outcome: }
}

func bootwith_transfer(s: KernelBootState, transfer: transfer_service.TransferServiceState) KernelBootState {
    return s with { transfer: service_cell_helpers.service_cell_with_state<transfer_service.TransferServiceState>(s.transfer, transfer) }
}

func bootwith_transfer_restart_outcome(s: KernelBootState, transfer_restart_outcome: RestartOutcome) KernelBootState {
    return s with { transfer_restart_outcome: }
}

func bootwith_ticket(s: KernelBootState, ticket: ticket_service.TicketServiceState) KernelBootState {
    return s with { ticket: service_cell_helpers.service_cell_with_state<ticket_service.TicketServiceState>(s.ticket, ticket) }
}

func bootwith_ticket_restart_outcome(s: KernelBootState, ticket_restart_outcome: RestartOutcome) KernelBootState {
    return s with { ticket_restart_outcome: }
}

func bootwith_grants(s: KernelBootState, grants: transfer_grant.GrantTable) KernelBootState {
    return s with { grants: }
}

func bootrestart_log(s: KernelBootState, log: log_service.LogServiceState) KernelBootState {
    return s with { log: service_cell_helpers.service_cell_restart<log_service.LogServiceState>(s.log, log) }
}

func bootrestart_kv(s: KernelBootState, kv: kv_service.KvServiceState) KernelBootState {
    return s with { kv: service_cell_helpers.service_cell_restart<kv_service.KvServiceState>(s.kv, kv) }
}

func bootrestart_queue(s: KernelBootState, queue: queue_service.QueueServiceState) KernelBootState {
    return s with { queue: service_cell_helpers.service_cell_restart<queue_service.QueueServiceState>(s.queue, queue) }
}

func bootrestart_workset_generation(s: KernelBootState) KernelBootState {
    return s with { workset_generation: s.workset_generation + 1 }
}

func bootrestart_audit_generation(s: KernelBootState) KernelBootState {
    return s with { audit_generation: s.audit_generation + 1 }
}

func bootrestart_echo(s: KernelBootState, echo: echo_service.EchoServiceState) KernelBootState {
    return s with { echo: service_cell_helpers.service_cell_restart<echo_service.EchoServiceState>(s.echo, echo) }
}

func bootrestart_transfer(s: KernelBootState, transfer: transfer_service.TransferServiceState) KernelBootState {
    return s with { transfer: service_cell_helpers.service_cell_restart<transfer_service.TransferServiceState>(s.transfer, transfer) }
}

func bootrestart_ticket(s: KernelBootState, ticket: ticket_service.TicketServiceState) KernelBootState {
    return s with { ticket: service_cell_helpers.service_cell_restart<ticket_service.TicketServiceState>(s.ticket, ticket) }
}

func bootwith_file(s: KernelBootState, file: file_service.FileServiceState) KernelBootState {
    return s with { file: service_cell_helpers.service_cell_with_state<file_service.FileServiceState>(s.file, file) }
}

func bootwith_file_restart_outcome(s: KernelBootState, file_restart_outcome: RestartOutcome) KernelBootState {
    return s with { file_restart_outcome: }
}

func bootrestart_file(s: KernelBootState, file: file_service.FileServiceState) KernelBootState {
    return s with { file: service_cell_helpers.service_cell_restart<file_service.FileServiceState>(s.file, file) }
}

func bootwith_timer(s: KernelBootState, timer: timer_service.TimerServiceState) KernelBootState {
    return s with { timer: service_cell_helpers.service_cell_with_state<timer_service.TimerServiceState>(s.timer, timer) }
}

func bootwith_timer_restart_outcome(s: KernelBootState, timer_restart_outcome: RestartOutcome) KernelBootState {
    return s with { timer_restart_outcome: }
}

func bootrestart_timer(s: KernelBootState, timer: timer_service.TimerServiceState) KernelBootState {
    return s with { timer: service_cell_helpers.service_cell_restart<timer_service.TimerServiceState>(s.timer, timer) }
}

func bootwith_task(s: KernelBootState, task: task_service.TaskServiceState) KernelBootState {
    return s with { task: service_cell_helpers.service_cell_with_state<task_service.TaskServiceState>(s.task, task) }
}

func bootwith_task_restart_outcome(s: KernelBootState, task_restart_outcome: RestartOutcome) KernelBootState {
    return s with { task_restart_outcome: }
}

func bootwith_connection(s: KernelBootState, connection: connection_service.ConnectionServiceState) KernelBootState {
    return s with { connection: service_cell_helpers.service_cell_with_state<connection_service.ConnectionServiceState>(s.connection, connection) }
}

func bootwith_connection_restart_outcome(s: KernelBootState, connection_restart_outcome: RestartOutcome) KernelBootState {
    return s with { connection_restart_outcome: }
}

func bootrestart_connection(s: KernelBootState, connection: connection_service.ConnectionServiceState) KernelBootState {
    return s with { connection: service_cell_helpers.service_cell_restart<connection_service.ConnectionServiceState>(s.connection, connection) }
}

func bootrestart_task(s: KernelBootState, task: task_service.TaskServiceState) KernelBootState {
    return s with { task: service_cell_helpers.service_cell_restart<task_service.TaskServiceState>(s.task, task) }
}

func bootwith_journal(s: KernelBootState, journal: journal_service.JournalServiceState) KernelBootState {
    return s with { journal: service_cell_helpers.service_cell_with_state<journal_service.JournalServiceState>(s.journal, journal) }
}

func bootwith_journal_restart_outcome(s: KernelBootState, journal_restart_outcome: RestartOutcome) KernelBootState {
    return s with { journal_restart_outcome: }
}

func bootrestart_journal(s: KernelBootState, journal: journal_service.JournalServiceState) KernelBootState {
    return s with { journal: service_cell_helpers.service_cell_restart<journal_service.JournalServiceState>(s.journal, journal) }
}

func bootwith_workflow(s: KernelBootState, workflow: workflow_core.WorkflowServiceState) KernelBootState {
    return s with { workflow: service_cell_helpers.service_cell_with_state<workflow_core.WorkflowServiceState>(s.workflow, workflow) }
}

func bootwith_lease(s: KernelBootState, lease: lease_service.LeaseServiceState) KernelBootState {
    return s with { lease: service_cell_helpers.service_cell_with_state<lease_service.LeaseServiceState>(s.lease, lease) }
}

func bootwith_lease_restart_outcome(s: KernelBootState, lease_restart_outcome: RestartOutcome) KernelBootState {
    return s with { lease_restart_outcome: }
}

func bootwith_completion(s: KernelBootState, completion: completion_mailbox_service.CompletionMailboxServiceState) KernelBootState {
    return s with { completion: service_cell_helpers.service_cell_with_state<completion_mailbox_service.CompletionMailboxServiceState>(s.completion, completion) }
}

func bootwith_completion_restart_outcome(s: KernelBootState, completion_restart_outcome: RestartOutcome) KernelBootState {
    return s with { completion_restart_outcome: }
}

func bootwith_object_store(s: KernelBootState, object_store: object_store_service.ObjectStoreServiceState) KernelBootState {
    return s with { object_store: service_cell_helpers.service_cell_with_state<object_store_service.ObjectStoreServiceState>(s.object_store, object_store) }
}

func bootwith_object_store_restart_outcome(s: KernelBootState, object_store_restart_outcome: RestartOutcome) KernelBootState {
    return s with { object_store_restart_outcome: }
}

func bootwith_update_store(s: KernelBootState, update_store: update_store_service.UpdateStoreServiceState) KernelBootState {
    return s with { update_store: service_cell_helpers.service_cell_with_state<update_store_service.UpdateStoreServiceState>(s.update_store, update_store) }
}

func bootwith_update_store_restart_outcome(s: KernelBootState, update_store_restart_outcome: RestartOutcome) KernelBootState {
    return s with { update_store_restart_outcome: }
}

func bootrestart_workflow(s: KernelBootState, workflow: workflow_core.WorkflowServiceState) KernelBootState {
    return s with { workflow: service_cell_helpers.service_cell_restart<workflow_core.WorkflowServiceState>(s.workflow, workflow) }
}

func bootrestart_lease(s: KernelBootState, lease: lease_service.LeaseServiceState) KernelBootState {
    return s with { lease: service_cell_helpers.service_cell_restart<lease_service.LeaseServiceState>(s.lease, lease) }
}

func bootrestart_completion(s: KernelBootState, completion: completion_mailbox_service.CompletionMailboxServiceState) KernelBootState {
    return s with { completion: service_cell_helpers.service_cell_restart<completion_mailbox_service.CompletionMailboxServiceState>(s.completion, completion) }
}

func bootrestart_object_store(s: KernelBootState, object_store: object_store_service.ObjectStoreServiceState) KernelBootState {
    return s with { object_store: service_cell_helpers.service_cell_restart<object_store_service.ObjectStoreServiceState>(s.object_store, object_store) }
}

func bootrestart_update_store(s: KernelBootState, update_store: update_store_service.UpdateStoreServiceState) KernelBootState {
    return s with { update_store: service_cell_helpers.service_cell_restart<update_store_service.UpdateStoreServiceState>(s.update_store, update_store) }
}
