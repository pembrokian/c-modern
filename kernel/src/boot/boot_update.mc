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
import service_identity
import task_service
import ticket_service
import timer_service
import transfer_grant
import transfer_service
import update_store_service
import workflow_service

func bootwith_log(s: KernelBootState, log: log_service.LogServiceState) KernelBootState {
    return s with { log.state: log }
}

func bootwith_path(s: KernelBootState, path: serial_shell_path.SerialShellPathState) KernelBootState {
    return s with { path_state: path }
}

func bootwith_kv(s: KernelBootState, kv: kv_service.KvServiceState) KernelBootState {
    return s with { kv.state: kv }
}

func bootwith_queue(s: KernelBootState, queue: queue_service.QueueServiceState) KernelBootState {
    return s with { queue.state: queue }
}

func bootwith_workset_generation(s: KernelBootState, generation: u32) KernelBootState {
    return s with { workset_generation: generation }
}

func bootwith_audit_generation(s: KernelBootState, generation: u32) KernelBootState {
    return s with { audit_generation: generation }
}

func bootwith_log_restart_outcome(s: KernelBootState, outcome: RestartOutcome) KernelBootState {
    return s with { log_restart_outcome: outcome }
}

func bootwith_kv_restart_outcome(s: KernelBootState, outcome: RestartOutcome) KernelBootState {
    return s with { kv_restart_outcome: outcome }
}

func bootwith_queue_restart_outcome(s: KernelBootState, outcome: RestartOutcome) KernelBootState {
    return s with { queue_restart_outcome: outcome }
}

func bootwith_workset_restart_outcome(s: KernelBootState, outcome: RestartOutcome) KernelBootState {
    return s with { workset_restart_outcome: outcome }
}

func bootwith_audit_restart_outcome(s: KernelBootState, outcome: RestartOutcome) KernelBootState {
    return s with { audit_restart_outcome: outcome }
}

func bootwith_echo(s: KernelBootState, echo: echo_service.EchoServiceState) KernelBootState {
    return s with { echo.state: echo }
}

func bootwith_echo_restart_outcome(s: KernelBootState, outcome: RestartOutcome) KernelBootState {
    return s with { echo_restart_outcome: outcome }
}

func bootwith_transfer(s: KernelBootState, transfer: transfer_service.TransferServiceState) KernelBootState {
    return s with { transfer.state: transfer }
}

func bootwith_transfer_restart_outcome(s: KernelBootState, outcome: RestartOutcome) KernelBootState {
    return s with { transfer_restart_outcome: outcome }
}

func bootwith_ticket(s: KernelBootState, ticket: ticket_service.TicketServiceState) KernelBootState {
    return s with { ticket.state: ticket }
}

func bootwith_ticket_restart_outcome(s: KernelBootState, outcome: RestartOutcome) KernelBootState {
    return s with { ticket_restart_outcome: outcome }
}

func bootwith_grants(s: KernelBootState, grants: transfer_grant.GrantTable) KernelBootState {
    return s with { grants: grants }
}

func bootrestart_log(s: KernelBootState, log: log_service.LogServiceState) KernelBootState {
    return s with { log.state: log, log.generation: s.log.generation + 1 }
}

func bootrestart_kv(s: KernelBootState, kv: kv_service.KvServiceState) KernelBootState {
    return s with { kv.state: kv, kv.generation: s.kv.generation + 1 }
}

func bootrestart_queue(s: KernelBootState, queue: queue_service.QueueServiceState) KernelBootState {
    return s with { queue.state: queue, queue.generation: s.queue.generation + 1 }
}

func bootrestart_workset_generation(s: KernelBootState) KernelBootState {
    return s with { workset_generation: s.workset_generation + 1 }
}

func bootrestart_audit_generation(s: KernelBootState) KernelBootState {
    return s with { audit_generation: s.audit_generation + 1 }
}

func bootrestart_echo(s: KernelBootState, echo: echo_service.EchoServiceState) KernelBootState {
    return s with { echo.state: echo, echo.generation: s.echo.generation + 1 }
}

func bootrestart_transfer(s: KernelBootState, transfer: transfer_service.TransferServiceState) KernelBootState {
    return s with { transfer.state: transfer, transfer.generation: s.transfer.generation + 1 }
}

func bootrestart_ticket(s: KernelBootState, ticket: ticket_service.TicketServiceState) KernelBootState {
    return s with { ticket.state: ticket, ticket.generation: s.ticket.generation + 1 }
}

func bootwith_file(s: KernelBootState, file: file_service.FileServiceState) KernelBootState {
    return s with { file.state: file }
}

func bootwith_file_restart_outcome(s: KernelBootState, outcome: RestartOutcome) KernelBootState {
    return s with { file_restart_outcome: outcome }
}

func bootrestart_file(s: KernelBootState, file: file_service.FileServiceState) KernelBootState {
    return s with { file.state: file, file.generation: s.file.generation + 1 }
}

func bootwith_timer(s: KernelBootState, timer: timer_service.TimerServiceState) KernelBootState {
    return s with { timer.state: timer }
}

func bootwith_timer_restart_outcome(s: KernelBootState, outcome: RestartOutcome) KernelBootState {
    return s with { timer_restart_outcome: outcome }
}

func bootrestart_timer(s: KernelBootState, timer: timer_service.TimerServiceState) KernelBootState {
    return s with { timer.state: timer, timer.generation: s.timer.generation + 1 }
}

func bootwith_task(s: KernelBootState, task: task_service.TaskServiceState) KernelBootState {
    return s with { task.state: task }
}

func bootwith_task_restart_outcome(s: KernelBootState, outcome: RestartOutcome) KernelBootState {
    return s with { task_restart_outcome: outcome }
}

func bootwith_connection(s: KernelBootState, connection: connection_service.ConnectionServiceState) KernelBootState {
    return s with { connection.state: connection }
}

func bootwith_connection_restart_outcome(s: KernelBootState, outcome: RestartOutcome) KernelBootState {
    return s with { connection_restart_outcome: outcome }
}

func bootrestart_connection(s: KernelBootState, connection: connection_service.ConnectionServiceState) KernelBootState {
    return s with { connection.state: connection, connection.generation: s.connection.generation + 1 }
}

func bootrestart_task(s: KernelBootState, task: task_service.TaskServiceState) KernelBootState {
    return s with { task.state: task, task.generation: s.task.generation + 1 }
}

func bootwith_journal(s: KernelBootState, journal: journal_service.JournalServiceState) KernelBootState {
    return s with { journal.state: journal }
}

func bootwith_journal_restart_outcome(s: KernelBootState, outcome: RestartOutcome) KernelBootState {
    return s with { journal_restart_outcome: outcome }
}

func bootrestart_journal(s: KernelBootState, journal: journal_service.JournalServiceState) KernelBootState {
    return s with { journal.state: journal, journal.generation: s.journal.generation + 1 }
}

func bootwith_workflow(s: KernelBootState, workflow: workflow_service.WorkflowServiceState) KernelBootState {
    return s with { workflow.state: workflow }
}

func bootwith_lease(s: KernelBootState, lease: lease_service.LeaseServiceState) KernelBootState {
    return s with { lease.state: lease }
}

func bootwith_lease_restart_outcome(s: KernelBootState, outcome: RestartOutcome) KernelBootState {
    return s with { lease_restart_outcome: outcome }
}

func bootwith_completion(s: KernelBootState, completion: completion_mailbox_service.CompletionMailboxServiceState) KernelBootState {
    return s with { completion.state: completion }
}

func bootwith_completion_restart_outcome(s: KernelBootState, outcome: RestartOutcome) KernelBootState {
    return s with { completion_restart_outcome: outcome }
}

func bootwith_object_store(s: KernelBootState, object_store: object_store_service.ObjectStoreServiceState) KernelBootState {
    return s with { object_store.state: object_store }
}

func bootwith_object_store_restart_outcome(s: KernelBootState, outcome: RestartOutcome) KernelBootState {
    return s with { object_store_restart_outcome: outcome }
}

func bootwith_update_store(s: KernelBootState, update_store: update_store_service.UpdateStoreServiceState) KernelBootState {
    return s with { update_store.state: update_store }
}

func bootwith_update_store_restart_outcome(s: KernelBootState, outcome: RestartOutcome) KernelBootState {
    return s with { update_store_restart_outcome: outcome }
}

func bootrestart_workflow(s: KernelBootState, workflow: workflow_service.WorkflowServiceState) KernelBootState {
    return s with { workflow.state: workflow, workflow.generation: s.workflow.generation + 1 }
}

func bootrestart_lease(s: KernelBootState, lease: lease_service.LeaseServiceState) KernelBootState {
    return s with { lease.state: lease, lease.generation: s.lease.generation + 1 }
}

func bootrestart_completion(s: KernelBootState, completion: completion_mailbox_service.CompletionMailboxServiceState) KernelBootState {
    return s with { completion.state: completion, completion.generation: s.completion.generation + 1 }
}

func bootrestart_object_store(s: KernelBootState, object_store: object_store_service.ObjectStoreServiceState) KernelBootState {
    return s with { object_store.state: object_store, object_store.generation: s.object_store.generation + 1 }
}

func bootrestart_update_store(s: KernelBootState, update_store: update_store_service.UpdateStoreServiceState) KernelBootState {
    return s with { update_store.state: update_store, update_store.generation: s.update_store.generation + 1 }
}
