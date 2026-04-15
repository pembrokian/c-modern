import boot
import init
import kernel_dispatch
import serial_protocol
import service_effect
import service_topology
import syscall

func cmd(payload: [4]u8) syscall.ReceiveObservation {
    return syscall.ReceiveObservation{ status: syscall.SyscallStatus.Ok, block_reason: syscall.BlockReason.None, endpoint_id: service_topology.SERIAL_ENDPOINT_ID, source_pid: 1, payload_len: 4, received_handle_slot: 0, received_handle_count: 0, payload: payload }
}

func tail_is(effect: service_effect.Effect, len: usize, a: u8, b: u8, c: u8) bool {
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != len {
        return false
    }
    payload: [4]u8 = service_effect.effect_reply_payload(effect)
    if len > 0 && payload[0] != a {
        return false
    }
    if len > 1 && payload[1] != b {
        return false
    }
    if len > 2 && payload[2] != c {
        return false
    }
    return true
}

func smoke_log_restart_reloads_retained_state() bool {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_log_append(21)))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_log_append(22)))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    state = init.restart(state, service_topology.LOG_ENDPOINT_ID)

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_log_tail()))
    if !tail_is(effect, 2, 21, 22, 0) {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_log_append(23)))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_log_tail()))
    if !tail_is(effect, 3, 21, 22, 23) {
        return false
    }

    return true
}

func main() i32 {
    if !smoke_log_restart_reloads_retained_state() {
        return 1
    }
    return 0
}