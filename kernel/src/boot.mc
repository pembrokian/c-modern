// Configuration, state construction, and service refs for the kernel boot
// image.  Owner PIDs and endpoint IDs live in service_topology.mc as
// ServiceSlot constants so that the full static wiring is named in one place.
//
// Dispatch and routing logic lives in kernel_dispatch.mc.

import kv_service
import log_service
import serial_service
import serial_shell_path
import service_effect
import service_identity
import service_topology
import shell_service
import syscall

struct KernelBootState {
    path_state: serial_shell_path.SerialShellPathState
    log_state: log_service.LogServiceState
    kv_state: kv_service.KvServiceState
}

func kernel_init() KernelBootState {
    serial_slot: service_topology.ServiceSlot = service_topology.SERIAL_SLOT
    shell_slot: service_topology.ServiceSlot = service_topology.SHELL_SLOT
    log_slot: service_topology.ServiceSlot = service_topology.LOG_SLOT
    kv_slot: service_topology.ServiceSlot = service_topology.KV_SLOT
    return KernelBootState{ path_state: serial_shell_path.path_init(serial_service.serial_init(serial_slot.pid, 1), shell_service.shell_init(shell_slot.pid, 1), shell_slot.endpoint), log_state: log_service.log_init(log_slot.pid, 1), kv_state: kv_service.kv_init(kv_slot.pid, 1) }
}

func bootwith_log(s: KernelBootState, log: log_service.LogServiceState) KernelBootState {
    return KernelBootState{ path_state: s.path_state, log_state: log, kv_state: s.kv_state }
}

func bootwith_kv(s: KernelBootState, kv: kv_service.KvServiceState) KernelBootState {
    return KernelBootState{ path_state: s.path_state, log_state: s.log_state, kv_state: kv }
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
