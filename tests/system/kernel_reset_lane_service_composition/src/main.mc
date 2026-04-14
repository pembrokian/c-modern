import boot
import ipc
import log_service
import service_effect
import syscall

const SERIAL_ENDPOINT_ID: u32 = 10

func build_serial_observation(b0: u8, b1: u8, b2: u8, b3: u8) syscall.ReceiveObservation {
    payload: [4]u8 = ipc.zero_payload()
    payload[0] = b0
    payload[1] = b1
    payload[2] = b2
    payload[3] = b3
    return syscall.ReceiveObservation{ status: syscall.SyscallStatus.Ok, block_reason: syscall.BlockReason.None, endpoint_id: SERIAL_ENDPOINT_ID, source_pid: 99, payload_len: 4, received_handle_slot: 0, received_handle_count: 0, payload: payload }
}

func smoke_mixed_shell_workflow_stays_explicit() bool {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect

    effect = boot.kernel_dispatch_step(&state, build_serial_observation(76, 65, 65, 33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    effect = boot.kernel_dispatch_step(&state, build_serial_observation(75, 83, 4, 9))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    effect = boot.kernel_dispatch_step(&state, build_serial_observation(75, 71, 4, 33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 2 {
        return false
    }
    if service_effect.effect_reply_payload(effect)[0] != 4 || service_effect.effect_reply_payload(effect)[1] != 9 {
        return false
    }

    effect = boot.kernel_dispatch_step(&state, build_serial_observation(76, 84, 33, 33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != log_service.debug_retained_len(state.log_state) {
        return false
    }
    if service_effect.effect_reply_payload(effect)[0] != 65 {
        return false
    }
    if service_effect.effect_reply_payload(effect)[1] != 75 {
        return false
    }
    return true
}

func main() i32 {
    if !smoke_mixed_shell_workflow_stays_explicit() {
        return 1
    }
    return 0
}