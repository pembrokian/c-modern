// Phase 184: capability propagation and leakage probe.
//
// Stable public endpoint names must not accidentally admit explicit
// authority-bearing metadata. Until real capability transfer exists,
// non-zero received-handle metadata is rejected at the dispatch boundary.

import boot
import init
import ipc
import kernel_dispatch
import service_effect
import service_identity
import service_topology
import syscall

func obs_for(endpoint: u32, slot: u32, count: usize) syscall.ReceiveObservation {
    return syscall.ReceiveObservation{ status: syscall.SyscallStatus.Ok, block_reason: syscall.BlockReason.None, endpoint_id: endpoint, source_pid: 41, payload_len: 0, received_handle_slot: slot, received_handle_count: count, payload: ipc.zero_payload() }
}

func rejected(effect: service_effect.Effect) bool {
    return service_effect.effect_reply_status(effect) == syscall.SyscallStatus.InvalidCapability
}

func check_handle_metadata_rejected_before_restart() bool {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(&state, obs_for(service_topology.LOG_ENDPOINT_ID, 7, 1))
    return rejected(effect)
}

func check_stale_handle_metadata_rejected_after_restart() bool {
    state: boot.KernelBootState = boot.kernel_init()
    log_ref: service_identity.ServiceRef = boot.BOOT_LOG_REF
    state = init.restart(state, service_identity.ref_endpoint(log_ref))

    effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(&state, obs_for(service_identity.ref_endpoint(log_ref), 7, 1))
    return rejected(effect)
}

func check_plain_named_traffic_survives_restart() bool {
    state: boot.KernelBootState = boot.kernel_init()
    log_ref: service_identity.ServiceRef = boot.BOOT_LOG_REF
    state = init.restart(state, service_identity.ref_endpoint(log_ref))

    effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(&state, obs_for(service_identity.ref_endpoint(log_ref), 0, 0))
    return service_effect.effect_reply_status(effect) == syscall.SyscallStatus.Ok
}

func check_repeated_hostile_attempts_stay_rejected() bool {
    state: boot.KernelBootState = boot.kernel_init()

    first: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(&state, obs_for(service_topology.KV_ENDPOINT_ID, 9, 1))
    second: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(&state, obs_for(service_topology.ECHO_ENDPOINT_ID, 11, 2))
    third: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(&state, obs_for(service_topology.LOG_ENDPOINT_ID, 13, 1))

    if !rejected(first) {
        return false
    }
    if !rejected(second) {
        return false
    }
    if !rejected(third) {
        return false
    }
    return true
}

func main() i32 {
    if !check_handle_metadata_rejected_before_restart() {
        return 1
    }
    if !check_stale_handle_metadata_rejected_after_restart() {
        return 2
    }
    if !check_plain_named_traffic_survives_restart() {
        return 3
    }
    if !check_repeated_hostile_attempts_stay_rejected() {
        return 4
    }
    return 0
}
