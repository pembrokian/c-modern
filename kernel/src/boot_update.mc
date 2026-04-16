import echo_service
import kv_service
import log_service
import queue_service
import serial_shell_path
import service_identity
import ticket_service
import transfer_grant
import transfer_service

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

func bootwith_echo(s: KernelBootState, echo: echo_service.EchoServiceState) KernelBootState {
    return s with { echo.state: echo }
}

func bootwith_transfer(s: KernelBootState, transfer: transfer_service.TransferServiceState) KernelBootState {
    return s with { transfer.state: transfer }
}

func bootwith_ticket(s: KernelBootState, ticket: ticket_service.TicketServiceState) KernelBootState {
    return s with { ticket.state: ticket }
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

func bootrestart_echo(s: KernelBootState, echo: echo_service.EchoServiceState) KernelBootState {
    return s with { echo.state: echo, echo.generation: s.echo.generation + 1 }
}

func bootrestart_transfer(s: KernelBootState, transfer: transfer_service.TransferServiceState) KernelBootState {
    return s with { transfer.state: transfer, transfer.generation: s.transfer.generation + 1 }
}

func bootrestart_ticket(s: KernelBootState, ticket: ticket_service.TicketServiceState) KernelBootState {
    return s with { ticket.state: ticket, ticket.generation: s.ticket.generation + 1 }
}