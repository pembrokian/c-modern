// Configuration, state construction, and service refs for the kernel boot
// image.  This module owns the service topology (endpoint IDs, owner PIDs,
// initial state) and exposes stable service references.
//
// Dispatch and routing logic lives in kernel_dispatch.mc.

import kv_service
import log_service
import serial_service
import serial_shell_path
import service_effect
import service_identity
import shell_service
import syscall

const SERIAL_ENDPOINT_ID: u32 = 10
const SHELL_ENDPOINT_ID: u32 = 11
const LOG_ENDPOINT_ID: u32 = 12
const KV_ENDPOINT_ID: u32 = 13

const BOOT_SERIAL_OWNER_PID: u32 = 1
const BOOT_SHELL_OWNER_PID: u32 = 2
const BOOT_LOG_OWNER_PID: u32 = 3
const BOOT_KV_OWNER_PID: u32 = 4

struct KernelBootState {
    path_state: serial_shell_path.SerialShellPathState
    log_state: log_service.LogServiceState
    kv_state: kv_service.KvServiceState
}

func kernel_init() KernelBootState {
    return KernelBootState{ path_state: serial_shell_path.path_init(serial_service.serial_init(BOOT_SERIAL_OWNER_PID, 1), shell_service.shell_init(BOOT_SHELL_OWNER_PID, 1, LOG_ENDPOINT_ID, KV_ENDPOINT_ID), SHELL_ENDPOINT_ID), log_state: log_service.log_init(BOOT_LOG_OWNER_PID, 1), kv_state: kv_service.kv_init(BOOT_KV_OWNER_PID, 1) }
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
    return service_identity.service_ref(SERIAL_ENDPOINT_ID)
}

func boot_shell_ref() service_identity.ServiceRef {
    return service_identity.service_ref(SHELL_ENDPOINT_ID)
}

func boot_log_ref() service_identity.ServiceRef {
    return service_identity.service_ref(LOG_ENDPOINT_ID)
}

func boot_kv_ref() service_identity.ServiceRef {
    return service_identity.service_ref(KV_ENDPOINT_ID)
}
