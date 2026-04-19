// Configuration, state construction, and service refs for the kernel boot
// image.  Owner PIDs and endpoint IDs live in service_topology.mc as
// ServiceSlot constants so that the full static wiring is named in one place.
//
// Dispatch and routing logic lives in kernel_dispatch.mc.

import completion_mailbox_service
import connection_service
import display_surface
import echo_service
import file_service
import issue_rollup_app
import journal_service
import kv_service
import launcher_service
import lease_service
import log_service
import object_store_service
import queue_service
import serial_service
import serial_shell_path
import service_cell_helpers
import service_effect
import service_identity
import service_topology
import shell_service
import syscall
import task_service
import ticket_service
import timer_service
import transfer_grant
import transfer_service
import update_store_service
import workflow/core

enum RetainedSummaryParticipation {
    None,
    Service,
    Lane,
}

enum RestartOutcome {
    None,
    OrdinaryReplaced,
    RetainedReloaded,
    CoordinatedRetainedReloaded,
    DurableReloaded,
}

struct KernelBootState {
    path_state: serial_shell_path.SerialShellPathState
    log: service_cell_helpers.ServiceCell<log_service.LogServiceState>
    kv: service_cell_helpers.ServiceCell<kv_service.KvServiceState>
    queue: service_cell_helpers.ServiceCell<queue_service.QueueServiceState>
    workset_generation: u32
    audit_generation: u32
    log_restart_outcome: RestartOutcome
    kv_restart_outcome: RestartOutcome
    queue_restart_outcome: RestartOutcome
    workset_restart_outcome: RestartOutcome
    audit_restart_outcome: RestartOutcome
    echo: service_cell_helpers.ServiceCell<echo_service.EchoServiceState>
    echo_restart_outcome: RestartOutcome
    transfer: service_cell_helpers.ServiceCell<transfer_service.TransferServiceState>
    transfer_restart_outcome: RestartOutcome
    ticket: service_cell_helpers.ServiceCell<ticket_service.TicketServiceState>
    ticket_restart_outcome: RestartOutcome
    file: service_cell_helpers.ServiceCell<file_service.FileServiceState>
    file_restart_outcome: RestartOutcome
    timer: service_cell_helpers.ServiceCell<timer_service.TimerServiceState>
    timer_restart_outcome: RestartOutcome
    task: service_cell_helpers.ServiceCell<task_service.TaskServiceState>
    task_restart_outcome: RestartOutcome
    connection: service_cell_helpers.ServiceCell<connection_service.ConnectionServiceState>
    connection_restart_outcome: RestartOutcome
    launcher: service_cell_helpers.ServiceCell<launcher_service.LauncherServiceState>
    issue_rollup: issue_rollup_app.IssueRollupAppState
    launcher_restart_outcome: RestartOutcome
    display: service_cell_helpers.ServiceCell<display_surface.DisplaySurfaceState>
    display_restart_outcome: RestartOutcome
    journal: service_cell_helpers.ServiceCell<journal_service.JournalServiceState>
    journal_restart_outcome: RestartOutcome
    workflow: service_cell_helpers.ServiceCell<workflow_core.WorkflowServiceState>
    lease: service_cell_helpers.ServiceCell<lease_service.LeaseServiceState>
    completion: service_cell_helpers.ServiceCell<completion_mailbox_service.CompletionMailboxServiceState>
    object_store: service_cell_helpers.ServiceCell<object_store_service.ObjectStoreServiceState>
    update_store: service_cell_helpers.ServiceCell<update_store_service.UpdateStoreServiceState>
    lease_restart_outcome: RestartOutcome
    completion_restart_outcome: RestartOutcome
    object_store_restart_outcome: RestartOutcome
    update_store_restart_outcome: RestartOutcome
    grants: transfer_grant.GrantTable
}

func kernel_init() KernelBootState {
    serial_slot := service_topology.SERIAL_SLOT
    shell_slot := service_topology.SHELL_SLOT
    log_slot := service_topology.LOG_SLOT
    kv_slot := service_topology.KV_SLOT
    queue_slot := service_topology.QUEUE_SLOT
    echo_slot := service_topology.ECHO_SLOT
    transfer_slot := service_topology.TRANSFER_SLOT
    ticket_slot := service_topology.TICKET_SLOT
    file_slot := service_topology.FILE_SLOT
    timer_slot := service_topology.TIMER_SLOT
    task_slot := service_topology.TASK_SLOT
    connection_slot := service_topology.CONNECTION_SLOT
    launcher_slot := service_topology.LAUNCHER_SLOT
    display_slot := service_topology.DISPLAY_SLOT
    journal_slot := service_topology.JOURNAL_SLOT
    workflow_slot := service_topology.WORKFLOW_SLOT
    lease_slot := service_topology.LEASE_SLOT
    completion_slot := service_topology.COMPLETION_MAILBOX_SLOT
    object_store_slot := service_topology.OBJECT_STORE_SLOT
    update_store_slot := service_topology.UPDATE_STORE_SLOT

    path_state := serial_shell_path.path_init(serial_service.serial_init(serial_slot.pid, 1), shell_service.shell_init(shell_slot.pid, 1), shell_slot.endpoint)
    log_cell := service_cell_helpers.ServiceCell<log_service.LogServiceState>{ state: log_service.log_init(log_slot.pid, 1), generation: 1 }
    kv_cell := service_cell_helpers.ServiceCell<kv_service.KvServiceState>{ state: kv_service.kv_init(kv_slot.pid, 1), generation: 1 }
    queue_cell := service_cell_helpers.ServiceCell<queue_service.QueueServiceState>{ state: queue_service.queue_init(queue_slot.pid, 1), generation: 1 }
    echo_cell := service_cell_helpers.ServiceCell<echo_service.EchoServiceState>{ state: echo_service.echo_init(echo_slot.pid, 1), generation: 1 }
    transfer_cell := service_cell_helpers.ServiceCell<transfer_service.TransferServiceState>{ state: transfer_service.transfer_init(transfer_slot.pid, 1), generation: 1 }
    ticket_cell := service_cell_helpers.ServiceCell<ticket_service.TicketServiceState>{ state: ticket_service.ticket_init(ticket_slot.pid, 1, 1), generation: 1 }
    file_cell := service_cell_helpers.ServiceCell<file_service.FileServiceState>{ state: file_service.file_init(file_slot.pid, 1), generation: 1 }
    timer_cell := service_cell_helpers.ServiceCell<timer_service.TimerServiceState>{ state: timer_service.timer_init(timer_slot.pid, 1), generation: 1 }
    task_cell := service_cell_helpers.ServiceCell<task_service.TaskServiceState>{ state: task_service.task_init(task_slot.pid, 1), generation: 1 }
    connection_cell := service_cell_helpers.ServiceCell<connection_service.ConnectionServiceState>{ state: connection_service.connection_init(connection_slot.pid, 1), generation: 1 }
    launcher_cell := service_cell_helpers.ServiceCell<launcher_service.LauncherServiceState>{ state: launcher_service.launcher_init(launcher_slot.pid, 1), generation: 1 }
    display_cell := service_cell_helpers.ServiceCell<display_surface.DisplaySurfaceState>{ state: display_surface.display_init(display_slot.pid, 1), generation: 1 }
    journal_cell := service_cell_helpers.ServiceCell<journal_service.JournalServiceState>{ state: journal_service.journal_load(journal_slot.pid, 1), generation: 1 }
    workflow_cell := service_cell_helpers.ServiceCell<workflow_core.WorkflowServiceState>{ state: workflow_core.workflow_state_init(workflow_slot.pid), generation: 1 }
    lease_cell := service_cell_helpers.ServiceCell<lease_service.LeaseServiceState>{ state: lease_service.lease_init(lease_slot.pid, 1), generation: 1 }
    completion_cell := service_cell_helpers.ServiceCell<completion_mailbox_service.CompletionMailboxServiceState>{ state: completion_mailbox_service.completion_mailbox_init(completion_slot.pid, 1), generation: 1 }
    object_store_cell := service_cell_helpers.ServiceCell<object_store_service.ObjectStoreServiceState>{ state: object_store_service.object_store_load(object_store_slot.pid, 1), generation: 1 }
    update_store_cell := service_cell_helpers.ServiceCell<update_store_service.UpdateStoreServiceState>{ state: update_store_service.update_store_load(update_store_slot.pid, 1), generation: 1 }

    return KernelBootState{
        path_state: path_state,
        log: log_cell,
        kv: kv_cell,
        queue: queue_cell,
        workset_generation: 1,
        audit_generation: 1,
        log_restart_outcome: RestartOutcome.None,
        kv_restart_outcome: RestartOutcome.None,
        queue_restart_outcome: RestartOutcome.None,
        workset_restart_outcome: RestartOutcome.None,
        audit_restart_outcome: RestartOutcome.None,
        echo: echo_cell,
        echo_restart_outcome: RestartOutcome.None,
        transfer: transfer_cell,
        transfer_restart_outcome: RestartOutcome.None,
        ticket: ticket_cell,
        ticket_restart_outcome: RestartOutcome.None,
        file: file_cell,
        file_restart_outcome: RestartOutcome.None,
        timer: timer_cell,
        timer_restart_outcome: RestartOutcome.None,
        task: task_cell,
        task_restart_outcome: RestartOutcome.None,
        connection: connection_cell,
        connection_restart_outcome: RestartOutcome.None,
        launcher: launcher_cell,
        issue_rollup: issue_rollup_app.issue_rollup_app_init(),
        launcher_restart_outcome: RestartOutcome.None,
        display: display_cell,
        display_restart_outcome: RestartOutcome.None,
        journal: journal_cell,
        journal_restart_outcome: RestartOutcome.None,
        workflow: workflow_cell,
        lease: lease_cell,
        completion: completion_cell,
        object_store: object_store_cell,
        update_store: update_store_cell,
        lease_restart_outcome: RestartOutcome.None,
        completion_restart_outcome: RestartOutcome.None,
        object_store_restart_outcome: RestartOutcome.None,
        update_store_restart_outcome: RestartOutcome.None,
        grants: transfer_grant.grant_init()
    }
}

func debug_boot_routed(effect: service_effect.Effect) u32 {
    if service_effect.effect_reply_status(effect) == syscall.SyscallStatus.InvalidEndpoint {
        return 0
    }
    return 1
}
