import boot
import ipc
import kernel_dispatch
import serial_service
import service_effect
import syscall

const SERIAL_ENDPOINT_ID: u32 = 10

func build_receive_observation(source_pid: u32, endpoint_id: u32, payload_len: usize, payload: [4]u8) syscall.ReceiveObservation {
    return syscall.ReceiveObservation{ status: syscall.SyscallStatus.Ok, block_reason: syscall.BlockReason.None, endpoint_id: endpoint_id, source_pid: source_pid, payload_len: payload_len, received_handle_slot: 0, received_handle_count: 0, payload: payload }
}

func smoke_echo_round_trip_passes() bool {
    payload: [4]u8 = ipc.zero_payload()
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect

    payload[0] = 69
    payload[1] = 67
    payload[2] = 72
    payload[3] = 73
    effect = kernel_dispatch.kernel_dispatch_step(&state, build_receive_observation(9, SERIAL_ENDPOINT_ID, 4, payload))
    if serial_service.serial_forwarded(state.path_state.serial_state) == 0 {
        return false
    }
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 2 {
        return false
    }
    reply_payload: [4]u8 = service_effect.effect_reply_payload(effect)
    if reply_payload[0] != 72 || reply_payload[1] != 73 {
        return false
    }
    if state.path_state.serial_state.len != 0 {
        return false
    }
    return true
}

func smoke_kv_set_logs_and_gets_value() bool {
    payload: [4]u8 = ipc.zero_payload()
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect

    payload[0] = 75
    payload[1] = 83
    payload[2] = 7
    payload[3] = 42
    effect = kernel_dispatch.kernel_dispatch_step(&state, build_receive_observation(9, SERIAL_ENDPOINT_ID, 4, payload))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    payload = ipc.zero_payload()
    payload[0] = 75
    payload[1] = 71
    payload[2] = 7
    payload[3] = 33
    effect = kernel_dispatch.kernel_dispatch_step(&state, build_receive_observation(9, SERIAL_ENDPOINT_ID, 4, payload))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 2 {
        return false
    }
    reply_payload: [4]u8 = service_effect.effect_reply_payload(effect)
    if reply_payload[0] != 7 || reply_payload[1] != 42 {
        return false
    }
    return true
}

func smoke_invalid_byte_does_not_forward() bool {
    payload: [4]u8 = ipc.zero_payload()
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect

    payload[0] = 255
    effect = kernel_dispatch.kernel_dispatch_step(&state, build_receive_observation(9, SERIAL_ENDPOINT_ID, 1, payload))
    if serial_service.serial_forwarded(state.path_state.serial_state) != 0 {
        return false
    }
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.None {
        return false
    }
    if state.path_state.serial_state.len != 0 {
        return false
    }
    return true
}

func main() i32 {
    if !smoke_echo_round_trip_passes() {
        return 1
    }
    if !smoke_kv_set_logs_and_gets_value() {
        return 1
    }
    if !smoke_invalid_byte_does_not_forward() {
        return 1
    }
    return 0
}