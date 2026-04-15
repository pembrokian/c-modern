// Configuration, state construction, and service refs for the kernel boot
// image.  Owner PIDs and endpoint IDs live in service_topology.mc as
// ServiceSlot constants so that the full static wiring is named in one place.
//
// Dispatch and routing logic lives in kernel_dispatch.mc.

import echo_service
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
import transfer_grant
import transfer_service

struct ServiceCell<T> {
    state: T
    generation: u32
}

struct KernelBootState {
    path_state: serial_shell_path.SerialShellPathState
    log: ServiceCell<log_service.LogServiceState>
    kv: ServiceCell<kv_service.KvServiceState>
    queue: ServiceCell<queue_service.QueueServiceState>
    echo: ServiceCell<echo_service.EchoServiceState>
    transfer: ServiceCell<transfer_service.TransferServiceState>
    grants: transfer_grant.GrantTable
}

func kernel_init() KernelBootState {
    serial_slot: service_topology.ServiceSlot = service_topology.SERIAL_SLOT
    shell_slot: service_topology.ServiceSlot = service_topology.SHELL_SLOT
    log_slot: service_topology.ServiceSlot = service_topology.LOG_SLOT
    kv_slot: service_topology.ServiceSlot = service_topology.KV_SLOT
    queue_slot: service_topology.ServiceSlot = service_topology.QUEUE_SLOT
    echo_slot: service_topology.ServiceSlot = service_topology.ECHO_SLOT
    transfer_slot: service_topology.ServiceSlot = service_topology.TRANSFER_SLOT

    path_state: serial_shell_path.SerialShellPathState = serial_shell_path.path_init(serial_service.serial_init(serial_slot.pid, 1), shell_service.shell_init(shell_slot.pid, 1), shell_slot.endpoint)
    log_cell: ServiceCell<log_service.LogServiceState> = ServiceCell<log_service.LogServiceState>{ state: log_service.log_init(log_slot.pid, 1), generation: 1 }
    kv_cell: ServiceCell<kv_service.KvServiceState> = ServiceCell<kv_service.KvServiceState>{ state: kv_service.kv_init(kv_slot.pid, 1), generation: 1 }
    queue_cell: ServiceCell<queue_service.QueueServiceState> = ServiceCell<queue_service.QueueServiceState>{ state: queue_service.queue_init(queue_slot.pid, 1), generation: 1 }
    echo_cell: ServiceCell<echo_service.EchoServiceState> = ServiceCell<echo_service.EchoServiceState>{ state: echo_service.echo_init(echo_slot.pid, 1), generation: 1 }
    transfer_cell: ServiceCell<transfer_service.TransferServiceState> = ServiceCell<transfer_service.TransferServiceState>{ state: transfer_service.transfer_init(transfer_slot.pid, 1), generation: 1 }

    return KernelBootState{ path_state: path_state, log: log_cell, kv: kv_cell, queue: queue_cell, echo: echo_cell, transfer: transfer_cell, grants: transfer_grant.grant_init() }
}

func bootwith_log(s: KernelBootState, log: log_service.LogServiceState) KernelBootState {
    return KernelBootState{ path_state: s.path_state, log: ServiceCell<log_service.LogServiceState>{ state: log, generation: s.log.generation }, kv: s.kv, queue: s.queue, echo: s.echo, transfer: s.transfer, grants: s.grants }
}

func bootwith_path(s: KernelBootState, path: serial_shell_path.SerialShellPathState) KernelBootState {
    return KernelBootState{ path_state: path, log: s.log, kv: s.kv, queue: s.queue, echo: s.echo, transfer: s.transfer, grants: s.grants }
}

func bootwith_kv(s: KernelBootState, kv: kv_service.KvServiceState) KernelBootState {
    return KernelBootState{ path_state: s.path_state, log: s.log, kv: ServiceCell<kv_service.KvServiceState>{ state: kv, generation: s.kv.generation }, queue: s.queue, echo: s.echo, transfer: s.transfer, grants: s.grants }
}

func bootwith_queue(s: KernelBootState, queue: queue_service.QueueServiceState) KernelBootState {
    return KernelBootState{ path_state: s.path_state, log: s.log, kv: s.kv, queue: ServiceCell<queue_service.QueueServiceState>{ state: queue, generation: s.queue.generation }, echo: s.echo, transfer: s.transfer, grants: s.grants }
}

func bootwith_echo(s: KernelBootState, echo: echo_service.EchoServiceState) KernelBootState {
    return KernelBootState{ path_state: s.path_state, log: s.log, kv: s.kv, queue: s.queue, echo: ServiceCell<echo_service.EchoServiceState>{ state: echo, generation: s.echo.generation }, transfer: s.transfer, grants: s.grants }
}

func bootwith_transfer(s: KernelBootState, transfer: transfer_service.TransferServiceState) KernelBootState {
    return KernelBootState{ path_state: s.path_state, log: s.log, kv: s.kv, queue: s.queue, echo: s.echo, transfer: ServiceCell<transfer_service.TransferServiceState>{ state: transfer, generation: s.transfer.generation }, grants: s.grants }
}

func bootwith_grants(s: KernelBootState, grants: transfer_grant.GrantTable) KernelBootState {
    return KernelBootState{ path_state: s.path_state, log: s.log, kv: s.kv, queue: s.queue, echo: s.echo, transfer: s.transfer, grants: grants }
}

func bootrestart_log(s: KernelBootState, log: log_service.LogServiceState) KernelBootState {
    return KernelBootState{ path_state: s.path_state, log: ServiceCell<log_service.LogServiceState>{ state: log, generation: s.log.generation + 1 }, kv: s.kv, queue: s.queue, echo: s.echo, transfer: s.transfer, grants: s.grants }
}

func bootrestart_kv(s: KernelBootState, kv: kv_service.KvServiceState) KernelBootState {
    return KernelBootState{ path_state: s.path_state, log: s.log, kv: ServiceCell<kv_service.KvServiceState>{ state: kv, generation: s.kv.generation + 1 }, queue: s.queue, echo: s.echo, transfer: s.transfer, grants: s.grants }
}

func bootrestart_queue(s: KernelBootState, queue: queue_service.QueueServiceState) KernelBootState {
    return KernelBootState{ path_state: s.path_state, log: s.log, kv: s.kv, queue: ServiceCell<queue_service.QueueServiceState>{ state: queue, generation: s.queue.generation + 1 }, echo: s.echo, transfer: s.transfer, grants: s.grants }
}

func bootrestart_echo(s: KernelBootState, echo: echo_service.EchoServiceState) KernelBootState {
    return KernelBootState{ path_state: s.path_state, log: s.log, kv: s.kv, queue: s.queue, echo: ServiceCell<echo_service.EchoServiceState>{ state: echo, generation: s.echo.generation + 1 }, transfer: s.transfer, grants: s.grants }
}

func bootrestart_transfer(s: KernelBootState, transfer: transfer_service.TransferServiceState) KernelBootState {
    return KernelBootState{ path_state: s.path_state, log: s.log, kv: s.kv, queue: s.queue, echo: s.echo, transfer: ServiceCell<transfer_service.TransferServiceState>{ state: transfer, generation: s.transfer.generation + 1 }, grants: s.grants }
}

func debug_boot_routed(effect: service_effect.Effect) u32 {
    if service_effect.effect_reply_status(effect) == syscall.SyscallStatus.InvalidEndpoint {
        return 0
    }
    return 1
}

// Named ServiceRef accessors for the four boot-wired services.
// These refs are stable across restart — the endpoint_id never changes after
// kernel_init() assigns it.  Callers that hold one of these refs may resume
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

func boot_log_mark(s: KernelBootState) service_identity.ServiceMark {
    return service_identity.service_mark(service_topology.LOG_ENDPOINT_ID, s.log.state.pid, s.log.generation)
}

func boot_kv_mark(s: KernelBootState) service_identity.ServiceMark {
    return service_identity.service_mark(service_topology.KV_ENDPOINT_ID, s.kv.state.pid, s.kv.generation)
}

func boot_queue_mark(s: KernelBootState) service_identity.ServiceMark {
    return service_identity.service_mark(service_topology.QUEUE_ENDPOINT_ID, s.queue.state.pid, s.queue.generation)
}

func boot_echo_mark(s: KernelBootState) service_identity.ServiceMark {
    return service_identity.service_mark(service_topology.ECHO_ENDPOINT_ID, s.echo.state.pid, s.echo.generation)
}

func boot_transfer_mark(s: KernelBootState) service_identity.ServiceMark {
    return service_identity.service_mark(service_topology.TRANSFER_ENDPOINT_ID, s.transfer.state.pid, s.transfer.generation)
}
