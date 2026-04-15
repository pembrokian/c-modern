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
    return KernelBootState{ path_state: s.path_state, log: ServiceCell<log_service.LogServiceState>{ state: log, generation: s.log.generation, generation_payload: s.log.generation_payload }, kv: s.kv, queue: s.queue, echo: s.echo, transfer: s.transfer, ticket: s.ticket, grants: s.grants }
}

func bootwith_path(s: KernelBootState, path: serial_shell_path.SerialShellPathState) KernelBootState {
    return KernelBootState{ path_state: path, log: s.log, kv: s.kv, queue: s.queue, echo: s.echo, transfer: s.transfer, ticket: s.ticket, grants: s.grants }
}

func bootwith_kv(s: KernelBootState, kv: kv_service.KvServiceState) KernelBootState {
    return KernelBootState{ path_state: s.path_state, log: s.log, kv: ServiceCell<kv_service.KvServiceState>{ state: kv, generation: s.kv.generation, generation_payload: s.kv.generation_payload }, queue: s.queue, echo: s.echo, transfer: s.transfer, ticket: s.ticket, grants: s.grants }
}

func bootwith_queue(s: KernelBootState, queue: queue_service.QueueServiceState) KernelBootState {
    return KernelBootState{ path_state: s.path_state, log: s.log, kv: s.kv, queue: ServiceCell<queue_service.QueueServiceState>{ state: queue, generation: s.queue.generation, generation_payload: s.queue.generation_payload }, echo: s.echo, transfer: s.transfer, ticket: s.ticket, grants: s.grants }
}

func bootwith_echo(s: KernelBootState, echo: echo_service.EchoServiceState) KernelBootState {
    return KernelBootState{ path_state: s.path_state, log: s.log, kv: s.kv, queue: s.queue, echo: ServiceCell<echo_service.EchoServiceState>{ state: echo, generation: s.echo.generation, generation_payload: s.echo.generation_payload }, transfer: s.transfer, ticket: s.ticket, grants: s.grants }
}

func bootwith_transfer(s: KernelBootState, transfer: transfer_service.TransferServiceState) KernelBootState {
    return KernelBootState{ path_state: s.path_state, log: s.log, kv: s.kv, queue: s.queue, echo: s.echo, transfer: ServiceCell<transfer_service.TransferServiceState>{ state: transfer, generation: s.transfer.generation, generation_payload: s.transfer.generation_payload }, ticket: s.ticket, grants: s.grants }
}

func bootwith_ticket(s: KernelBootState, ticket: ticket_service.TicketServiceState) KernelBootState {
    return KernelBootState{ path_state: s.path_state, log: s.log, kv: s.kv, queue: s.queue, echo: s.echo, transfer: s.transfer, ticket: ServiceCell<ticket_service.TicketServiceState>{ state: ticket, generation: s.ticket.generation, generation_payload: s.ticket.generation_payload }, grants: s.grants }
}

func bootwith_grants(s: KernelBootState, grants: transfer_grant.GrantTable) KernelBootState {
    return KernelBootState{ path_state: s.path_state, log: s.log, kv: s.kv, queue: s.queue, echo: s.echo, transfer: s.transfer, ticket: s.ticket, grants: grants }
}

func bootrestart_log(s: KernelBootState, log: log_service.LogServiceState) KernelBootState {
    return KernelBootState{ path_state: s.path_state, log: ServiceCell<log_service.LogServiceState>{ state: log, generation: s.log.generation + 1, generation_payload: service_identity.generation_next_payload(s.log.generation_payload) }, kv: s.kv, queue: s.queue, echo: s.echo, transfer: s.transfer, ticket: s.ticket, grants: s.grants }
}

func bootrestart_kv(s: KernelBootState, kv: kv_service.KvServiceState) KernelBootState {
    return KernelBootState{ path_state: s.path_state, log: s.log, kv: ServiceCell<kv_service.KvServiceState>{ state: kv, generation: s.kv.generation + 1, generation_payload: service_identity.generation_next_payload(s.kv.generation_payload) }, queue: s.queue, echo: s.echo, transfer: s.transfer, ticket: s.ticket, grants: s.grants }
}

func bootrestart_queue(s: KernelBootState, queue: queue_service.QueueServiceState) KernelBootState {
    return KernelBootState{ path_state: s.path_state, log: s.log, kv: s.kv, queue: ServiceCell<queue_service.QueueServiceState>{ state: queue, generation: s.queue.generation + 1, generation_payload: service_identity.generation_next_payload(s.queue.generation_payload) }, echo: s.echo, transfer: s.transfer, ticket: s.ticket, grants: s.grants }
}

func bootrestart_echo(s: KernelBootState, echo: echo_service.EchoServiceState) KernelBootState {
    return KernelBootState{ path_state: s.path_state, log: s.log, kv: s.kv, queue: s.queue, echo: ServiceCell<echo_service.EchoServiceState>{ state: echo, generation: s.echo.generation + 1, generation_payload: service_identity.generation_next_payload(s.echo.generation_payload) }, transfer: s.transfer, ticket: s.ticket, grants: s.grants }
}

func bootrestart_transfer(s: KernelBootState, transfer: transfer_service.TransferServiceState) KernelBootState {
    return KernelBootState{ path_state: s.path_state, log: s.log, kv: s.kv, queue: s.queue, echo: s.echo, transfer: ServiceCell<transfer_service.TransferServiceState>{ state: transfer, generation: s.transfer.generation + 1, generation_payload: service_identity.generation_next_payload(s.transfer.generation_payload) }, ticket: s.ticket, grants: s.grants }
}

func bootrestart_ticket(s: KernelBootState, ticket: ticket_service.TicketServiceState) KernelBootState {
    return KernelBootState{ path_state: s.path_state, log: s.log, kv: s.kv, queue: s.queue, echo: s.echo, transfer: s.transfer, ticket: ServiceCell<ticket_service.TicketServiceState>{ state: ticket, generation: s.ticket.generation + 1, generation_payload: service_identity.generation_next_payload(s.ticket.generation_payload) }, grants: s.grants }
}