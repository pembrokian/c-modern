import boot
import ipc
import kernel_dispatch
import serial_protocol
import service_effect
import service_identity
import service_topology
import shell_service
import syscall

func cmd(payload: [4]u8) syscall.ReceiveObservation {
    return syscall.ReceiveObservation{ status: syscall.SyscallStatus.Ok, block_reason: syscall.BlockReason.None, endpoint_id: service_topology.SERIAL_ENDPOINT_ID, source_pid: 1, payload_len: 4, received_handle_slot: 0, received_handle_count: 0, payload: payload }
}

func expect_identity(effect: service_effect.Effect, status: syscall.SyscallStatus, mark: service_identity.ServiceMark) bool {
    if service_effect.effect_reply_status(effect) != status {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 4 {
        return false
    }
    payload: [4]u8 = service_effect.effect_reply_payload(effect)
    expected: [4]u8 = service_identity.mark_generation_payload(mark)
    if payload[0] != expected[0] {
        return false
    }
    if payload[1] != expected[1] {
        return false
    }
    if payload[2] != expected[2] {
        return false
    }
    if payload[3] != expected[3] {
        return false
    }
    return true
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

func expect_restart_identity(before: service_identity.ServiceMark, after: service_identity.ServiceMark) bool {
    if !service_identity.marks_same_endpoint(before, after) {
        return false
    }
    if !service_identity.marks_same_pid(before, after) {
        return false
    }
    if service_identity.marks_same_instance(before, after) {
        return false
    }
    if service_identity.mark_generation(after) != service_identity.mark_generation(before) + 1 {
        return false
    }
    return true
}

func smoke_reload_identity_query() bool {
    state: boot.KernelBootState = boot.kernel_init()
    before: service_identity.ServiceMark = boot.boot_queue_mark(state)
    effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_lifecycle_identity(serial_protocol.TARGET_QUEUE)))
    if !expect_identity(effect, syscall.SyscallStatus.Ok, before) {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_enqueue(21)))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_enqueue(22)))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_lifecycle_restart(serial_protocol.TARGET_QUEUE)))
    if !expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_QUEUE, serial_protocol.LIFECYCLE_RELOAD) {
        return false
    }

    after: service_identity.ServiceMark = boot.boot_queue_mark(state)
    if !expect_restart_identity(before, after) {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_lifecycle_identity(serial_protocol.TARGET_QUEUE)))
    if !expect_identity(effect, syscall.SyscallStatus.Ok, after) {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_dequeue()))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload(effect)[0] != 21 {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_dequeue()))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload(effect)[0] != 22 {
        return false
    }
    return true
}

func smoke_reset_identity_query() bool {
    state: boot.KernelBootState = boot.kernel_init()
    before: service_identity.ServiceMark = boot.boot_echo_mark(state)
    effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_lifecycle_identity(serial_protocol.TARGET_ECHO)))
    if !expect_identity(effect, syscall.SyscallStatus.Ok, before) {
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

    after: service_identity.ServiceMark = boot.boot_echo_mark(state)
    if !expect_restart_identity(before, after) {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_lifecycle_identity(serial_protocol.TARGET_ECHO)))
    if !expect_identity(effect, syscall.SyscallStatus.Ok, after) {
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

func smoke_hostile_identity_input_stays_shell_owned() bool {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(ipc.payload_byte(serial_protocol.CMD_X, serial_protocol.CMD_I, serial_protocol.CMD_BANG, serial_protocol.CMD_BANG)))
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
    if !smoke_reload_identity_query() {
        return 1
    }
    if !smoke_reset_identity_query() {
        return 2
    }
    if !smoke_hostile_identity_input_stays_shell_owned() {
        return 3
    }
    return 0
}