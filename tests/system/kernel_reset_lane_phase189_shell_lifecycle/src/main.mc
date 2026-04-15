import boot
import ipc
import kernel_dispatch
import serial_protocol
import service_effect
import service_topology
import shell_service
import syscall

func cmd(payload: [4]u8) syscall.ReceiveObservation {
    return syscall.ReceiveObservation{ status: syscall.SyscallStatus.Ok, block_reason: syscall.BlockReason.None, endpoint_id: service_topology.SERIAL_ENDPOINT_ID, source_pid: 1, payload_len: 4, received_handle_slot: 0, received_handle_count: 0, payload: payload }
}

func expect_lifecycle(effect: service_effect.Effect, status: syscall.SyscallStatus, target: u8, mode: u8) bool {
    if service_effect.effect_reply_status(effect) != status {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 2 {
        return false
    }
    payload: [4]u8 = service_effect.effect_reply_payload(effect)
    if payload[0] != target {
        return false
    }
    if payload[1] != mode {
        return false
    }
    return true
}

func smoke_shell_queries_lifecycle_classes() bool {
    state: boot.KernelBootState = boot.kernel_init()

    effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_lifecycle_query(serial_protocol.TARGET_LOG)))
    if !expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_LOG, serial_protocol.LIFECYCLE_RELOAD) {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_lifecycle_query(serial_protocol.TARGET_ECHO)))
    if !expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_ECHO, serial_protocol.LIFECYCLE_RESET) {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_lifecycle_query(serial_protocol.TARGET_SERIAL)))
    if !expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_SERIAL, serial_protocol.LIFECYCLE_NONE) {
        return false
    }

    return true
}

func smoke_non_restartable_target_is_rejected() bool {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_lifecycle_restart(serial_protocol.TARGET_SHELL)))
    return expect_lifecycle(effect, syscall.SyscallStatus.InvalidArgument, serial_protocol.TARGET_SHELL, serial_protocol.LIFECYCLE_NONE)
}

func smoke_reload_and_reset_follow_through() bool {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_enqueue(61)))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_enqueue(62)))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_lifecycle_restart(serial_protocol.TARGET_QUEUE)))
    if !expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_QUEUE, serial_protocol.LIFECYCLE_RELOAD) {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_dequeue()))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload(effect)[0] != 61 {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_dequeue()))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload(effect)[0] != 62 {
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

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_lifecycle_restart(serial_protocol.TARGET_ECHO)))
    if !expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_ECHO, serial_protocol.LIFECYCLE_RESET) {
        return false
    }

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

func smoke_hostile_lifecycle_input_stays_shell_owned() bool {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(ipc.payload_byte(serial_protocol.CMD_X, serial_protocol.CMD_G, serial_protocol.TARGET_LOG, serial_protocol.CMD_BANG)))

    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.InvalidArgument {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 2 {
        return false
    }
    if service_effect.effect_reply_payload(effect)[0] != shell_service.SHELL_INVALID_REPLY {
        return false
    }
    if service_effect.effect_reply_payload(effect)[1] != shell_service.SHELL_INVALID_COMMAND {
        return false
    }
    return true
}

func main() i32 {
    if !smoke_shell_queries_lifecycle_classes() {
        return 1
    }
    if !smoke_non_restartable_target_is_rejected() {
        return 2
    }
    if !smoke_reload_and_reset_follow_through() {
        return 3
    }
    if !smoke_hostile_lifecycle_input_stays_shell_owned() {
        return 4
    }
    return 0
}