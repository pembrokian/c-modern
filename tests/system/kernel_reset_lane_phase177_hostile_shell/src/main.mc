import boot
import init
import ipc
import kernel_dispatch
import serial_protocol
import service_effect
import shell_service
import service_topology
import syscall

func cmd(payload: [4]u8) syscall.ReceiveObservation {
    return syscall.ReceiveObservation{ status: syscall.SyscallStatus.Ok, block_reason: syscall.BlockReason.None, endpoint_id: service_topology.SERIAL_ENDPOINT_ID, source_pid: 1, payload_len: 4, received_handle_slot: 0, received_handle_count: 0, payload: payload }
}

func expect_shell_invalid(effect: service_effect.Effect, kind: u8) bool {
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.InvalidArgument {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 2 {
        return false
    }
    if service_effect.effect_reply_payload(effect)[0] != shell_service.SHELL_INVALID_REPLY {
        return false
    }
    if service_effect.effect_reply_payload(effect)[1] != kind {
        return false
    }
    return true
}

func smoke_unknown_command_is_classified() bool {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(ipc.payload_byte(serial_protocol.CMD_L, serial_protocol.CMD_G, 7, serial_protocol.CMD_BANG)))
    return expect_shell_invalid(effect, shell_service.SHELL_INVALID_COMMAND)
}

func smoke_malformed_shape_is_classified() bool {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(ipc.payload_byte(serial_protocol.CMD_K, serial_protocol.CMD_G, 5, 0)))
    return expect_shell_invalid(effect, shell_service.SHELL_INVALID_SHAPE)
}

func smoke_repeated_invalid_input_stays_non_mutating() bool {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(ipc.payload_byte(serial_protocol.CMD_L, serial_protocol.CMD_G, 1, serial_protocol.CMD_BANG)))
    if !expect_shell_invalid(effect, shell_service.SHELL_INVALID_COMMAND) {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(ipc.payload_byte(serial_protocol.CMD_K, serial_protocol.CMD_G, 9, 0)))
    if !expect_shell_invalid(effect, shell_service.SHELL_INVALID_SHAPE) {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_log_tail()))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 0 {
        return false
    }
    return true
}

func smoke_mixed_valid_and_invalid_sequence_preserves_valid_state() bool {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_kv_set(5, 44)))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(ipc.payload_byte(serial_protocol.CMD_E, serial_protocol.CMD_T, 1, 2)))
    if !expect_shell_invalid(effect, shell_service.SHELL_INVALID_COMMAND) {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_kv_get(5)))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 2 {
        return false
    }
    if service_effect.effect_reply_payload(effect)[1] != 44 {
        return false
    }
    return true
}

func smoke_service_failures_stay_service_owned() bool {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_kv_get(99)))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.InvalidArgument {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 0 {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_echo(1, 2)))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_echo(3, 4)))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_echo(5, 6)))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_echo(7, 8)))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_echo(9, 10)))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Exhausted {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 0 {
        return false
    }
    return true
}

func smoke_restart_preserves_stateful_boundary() bool {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_echo(1, 2)))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_echo(3, 4)))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_echo(5, 6)))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_echo(7, 8)))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(ipc.payload_byte(serial_protocol.CMD_E, serial_protocol.CMD_T, 1, 2)))
    if !expect_shell_invalid(effect, shell_service.SHELL_INVALID_COMMAND) {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_echo(9, 10)))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Exhausted {
        return false
    }

    state = init.restart_echo(state)
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_echo(11, 12)))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload(effect)[0] != 11 {
        return false
    }
    if service_effect.effect_reply_payload(effect)[1] != 12 {
        return false
    }
    return true
}

func main() i32 {
    if !smoke_unknown_command_is_classified() {
        return 1
    }
    if !smoke_malformed_shape_is_classified() {
        return 2
    }
    if !smoke_repeated_invalid_input_stays_non_mutating() {
        return 3
    }
    if !smoke_mixed_valid_and_invalid_sequence_preserves_valid_state() {
        return 4
    }
    if !smoke_service_failures_stay_service_owned() {
        return 5
    }
    if !smoke_restart_preserves_stateful_boundary() {
        return 6
    }
    return 0
}