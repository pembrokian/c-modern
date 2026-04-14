import boot
import ipc
import kv_service
import log_service
import serial_service
import service_effect
import syscall

const SERIAL_ENDPOINT_ID: u32 = 10
const LOG_ENDPOINT_ID: u32 = 12
const KV_ENDPOINT_ID: u32 = 13

func build_observation(endpoint_id: u32, payload_len: usize, payload: [4]u8) syscall.ReceiveObservation {
    return syscall.ReceiveObservation{ status: syscall.SyscallStatus.Ok, block_reason: syscall.BlockReason.None, endpoint_id: endpoint_id, source_pid: 99, payload_len: payload_len, received_handle_slot: 0, received_handle_count: 0, payload: payload }
}

func smoke_init_has_empty_services() bool {
    state: boot.KernelBootState = boot.kernel_init()
    if log_service.debug_retained_len(state.log_state) != 0 {
        return false
    }
    if kv_service.debug_kv_count(state.kv_state) != 0 {
        return false
    }
    return true
}

func smoke_serial_dispatch_routes() bool {
    state: boot.KernelBootState = boot.kernel_init()
    payload: [4]u8 = ipc.zero_payload()
    effect: service_effect.Effect

    payload[0] = 69
    payload[1] = 67
    payload[2] = 65
    payload[3] = 66
    effect = boot.kernel_dispatch_step(&state, build_observation(SERIAL_ENDPOINT_ID, 4, payload))
    if boot.debug_boot_routed(effect) == 0 {
        return false
    }
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if serial_service.serial_forwarded(state.path_state.serial_state) == 0 {
        return false
    }
    return true
}

func smoke_serial_composes_services() bool {
    state: boot.KernelBootState = boot.kernel_init()
    payload: [4]u8 = ipc.zero_payload()
    effect: service_effect.Effect

    payload[0] = 76
    payload[1] = 65
    payload[2] = 77
    payload[3] = 33
    effect = boot.kernel_dispatch_step(&state, build_observation(SERIAL_ENDPOINT_ID, 4, payload))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    payload[0] = 75
    payload[1] = 83
    payload[2] = 5
    payload[3] = 42
    effect = boot.kernel_dispatch_step(&state, build_observation(SERIAL_ENDPOINT_ID, 4, payload))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    payload[0] = 76
    payload[1] = 84
    payload[2] = 33
    payload[3] = 33
    effect = boot.kernel_dispatch_step(&state, build_observation(SERIAL_ENDPOINT_ID, 4, payload))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 2 {
        return false
    }
    if service_effect.effect_reply_payload(effect)[0] != 77 || service_effect.effect_reply_payload(effect)[1] != 75 {
        return false
    }
    if log_service.debug_retained_len(state.log_state) != 2 {
        return false
    }
    return true
}

func smoke_log_dispatch_routes() bool {
    state: boot.KernelBootState = boot.kernel_init()
    payload: [4]u8 = ipc.zero_payload()
    effect: service_effect.Effect

    payload[0] = 77
    effect = boot.kernel_dispatch_step(&state, build_observation(LOG_ENDPOINT_ID, 1, payload))
    if boot.debug_boot_routed(effect) == 0 {
        return false
    }
    if log_service.debug_retained_len(state.log_state) != 1 {
        return false
    }
    return true
}

func smoke_kv_dispatch_routes() bool {
    state: boot.KernelBootState = boot.kernel_init()
    payload: [4]u8 = ipc.zero_payload()
    effect: service_effect.Effect

    payload[0] = 5
    payload[1] = 42
    effect = boot.kernel_dispatch_step(&state, build_observation(KV_ENDPOINT_ID, 2, payload))
    if boot.debug_boot_routed(effect) == 0 {
        return false
    }
    if kv_service.debug_kv_count(state.kv_state) != 1 {
        return false
    }
    return true
}

func smoke_unknown_endpoint_not_routed() bool {
    state: boot.KernelBootState = boot.kernel_init()
    payload: [4]u8 = ipc.zero_payload()
    effect: service_effect.Effect

    effect = boot.kernel_dispatch_step(&state, build_observation(99, 0, payload))
    if boot.debug_boot_routed(effect) != 0 {
        return false
    }
    return true
}

func main() i32 {
    if !smoke_init_has_empty_services() {
        return 1
    }
    if !smoke_serial_dispatch_routes() {
        return 1
    }
    if !smoke_serial_composes_services() {
        return 1
    }
    if !smoke_log_dispatch_routes() {
        return 1
    }
    if !smoke_kv_dispatch_routes() {
        return 1
    }
    if !smoke_unknown_endpoint_not_routed() {
        return 1
    }
    return 0
}
