// Configuration, state construction, and service refs for the kernel boot
// image.  Owner PIDs and endpoint IDs live in service_topology.mc as
// ServiceSlot constants so that the full static wiring is named in one place.
//
// Dispatch and routing logic lives in kernel_dispatch.mc.

import echo_service
import file_service
import kv_service
import log_service
import queue_service
import serial_service
import serial_shell_path
import service_effect
import service_identity
import service_topology
import shell_service
import syscall
import ticket_service
import transfer_grant
import transfer_service

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
}

struct ServiceCell<T> {
    state: T
    generation: u32
}

struct KernelBootState {
    path_state: serial_shell_path.SerialShellPathState
    log: ServiceCell<log_service.LogServiceState>
    kv: ServiceCell<kv_service.KvServiceState>
    queue: ServiceCell<queue_service.QueueServiceState>
    workset_generation: u32
    audit_generation: u32
    log_restart_outcome: RestartOutcome
    kv_restart_outcome: RestartOutcome
    queue_restart_outcome: RestartOutcome
    workset_restart_outcome: RestartOutcome
    audit_restart_outcome: RestartOutcome
    echo: ServiceCell<echo_service.EchoServiceState>
    echo_restart_outcome: RestartOutcome
    transfer: ServiceCell<transfer_service.TransferServiceState>
    transfer_restart_outcome: RestartOutcome
    ticket: ServiceCell<ticket_service.TicketServiceState>
    ticket_restart_outcome: RestartOutcome
    file: ServiceCell<file_service.FileServiceState>
    file_restart_outcome: RestartOutcome
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

    path_state := serial_shell_path.path_init(serial_service.serial_init(serial_slot.pid, 1), shell_service.shell_init(shell_slot.pid, 1), shell_slot.endpoint)
    log_cell := ServiceCell<log_service.LogServiceState>{ state: log_service.log_init(log_slot.pid, 1), generation: 1 }
    kv_cell := ServiceCell<kv_service.KvServiceState>{ state: kv_service.kv_init(kv_slot.pid, 1), generation: 1 }
    queue_cell := ServiceCell<queue_service.QueueServiceState>{ state: queue_service.queue_init(queue_slot.pid, 1), generation: 1 }
    echo_cell := ServiceCell<echo_service.EchoServiceState>{ state: echo_service.echo_init(echo_slot.pid, 1), generation: 1 }
    transfer_cell := ServiceCell<transfer_service.TransferServiceState>{ state: transfer_service.transfer_init(transfer_slot.pid, 1), generation: 1 }
    ticket_cell := ServiceCell<ticket_service.TicketServiceState>{ state: ticket_service.ticket_init(ticket_slot.pid, 1, 1), generation: 1 }
    file_cell := ServiceCell<file_service.FileServiceState>{ state: file_service.file_init(file_slot.pid, 1), generation: 1 }

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
        grants: transfer_grant.grant_init()
    }
}

func debug_boot_routed(effect: service_effect.Effect) u32 {
    if service_effect.effect_reply_status(effect) == syscall.SyscallStatus.InvalidEndpoint {
        return 0
    }
    return 1
}
