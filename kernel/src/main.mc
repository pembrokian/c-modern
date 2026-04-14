import boot
import primitives
import log_service
import service_effect
import syscall

const SERIAL_ENDPOINT_ID: u32 = 10  // mirrors boot.SERIAL_ENDPOINT_ID

func make_serial_command(b0: u8, b1: u8, b2: u8, b3: u8) syscall.ReceiveObservation {
    payload: [4]u8 = primitives.zero_payload()
    payload[0] = b0
    payload[1] = b1
    payload[2] = b2
    payload[3] = b3
    return syscall.ReceiveObservation{ status: syscall.SyscallStatus.Ok, block_reason: syscall.BlockReason.None, endpoint_id: SERIAL_ENDPOINT_ID, source_pid: 1, payload_len: 4, received_handle_slot: 0, received_handle_count: 0, payload: payload }
}

// Named command builders encode protocol intent; bytes are ASCII shell protocol.
func cmd_log_append(value: u8) syscall.ReceiveObservation {
    return make_serial_command(76, 65, value, 33)  // L A <value> !
}

func cmd_kv_set(key: u8, value: u8) syscall.ReceiveObservation {
    return make_serial_command(75, 83, key, value)  // K S <key> <value>
}

func cmd_kv_get(key: u8) syscall.ReceiveObservation {
    return make_serial_command(75, 71, key, 33)  // K G <key> !
}

func cmd_log_tail() syscall.ReceiveObservation {
    return make_serial_command(76, 84, 33, 33)  // L T ! !
}

func main() i32 {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect

    effect = boot.kernel_dispatch_step(&state, cmd_log_append(77))
    if boot.debug_boot_routed(effect) == 0 {
        return 1
    }

    effect = boot.kernel_dispatch_step(&state, cmd_kv_set(5, 42))
    if boot.debug_boot_routed(effect) == 0 {
        return 1
    }

    effect = boot.kernel_dispatch_step(&state, cmd_kv_get(5))
    if boot.debug_boot_routed(effect) == 0 {
        return 1
    }
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return 1
    }
    if service_effect.effect_reply_payload(effect)[1] != 42 {
        return 1
    }

    effect = boot.kernel_dispatch_step(&state, cmd_log_tail())
    if boot.debug_boot_routed(effect) == 0 {
        return 1
    }
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return 1
    }
    if service_effect.effect_reply_payload_len(effect) != log_service.log_len(state.log_state) {
        return 1
    }
    if service_effect.effect_reply_payload(effect)[0] != 77 {
        return 1
    }
    if service_effect.effect_reply_payload(effect)[1] != 75 {
        return 1
    }

    return 0
}
