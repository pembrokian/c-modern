import boot
import kernel_dispatch
import serial_protocol
import service_effect
import service_topology
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

func queue_count_is(effect: service_effect.Effect, count: usize) bool {
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != count {
        return false
    }
    return true
}

func queue_peek_is(effect: service_effect.Effect, value: u8) bool {
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 1 {
        return false
    }
    if service_effect.effect_reply_payload(effect)[0] != value {
        return false
    }
    return true
}

func queue_status_is(effect: service_effect.Effect, status: syscall.SyscallStatus) bool {
    if service_effect.effect_reply_status(effect) != status {
        return false
    }
    return true
}

func smoke_queue_observability_survives_reload() bool {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_count()))
    if !queue_count_is(effect, 0) {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_peek()))
    if !queue_status_is(effect, syscall.SyscallStatus.InvalidArgument) {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_enqueue(10)))
    if !queue_status_is(effect, syscall.SyscallStatus.Ok) {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_enqueue(20)))
    if !queue_status_is(effect, syscall.SyscallStatus.Ok) {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_count()))
    if !queue_count_is(effect, 2) {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_peek()))
    if !queue_peek_is(effect, 10) {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_peek()))
    if !queue_peek_is(effect, 10) {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_dequeue()))
    if !queue_peek_is(effect, 10) {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_peek()))
    if !queue_peek_is(effect, 20) {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_lifecycle_restart(serial_protocol.TARGET_QUEUE)))
    if !expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_QUEUE, serial_protocol.LIFECYCLE_RELOAD) {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_count()))
    if !queue_count_is(effect, 1) {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_peek()))
    if !queue_peek_is(effect, 20) {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_dequeue()))
    if !queue_peek_is(effect, 20) {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_count()))
    if !queue_count_is(effect, 0) {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_peek()))
    if !queue_status_is(effect, syscall.SyscallStatus.InvalidArgument) {
        return false
    }

    return true
}

func main() i32 {
    if !smoke_queue_observability_survives_reload() {
        return 1
    }
    return 0
}