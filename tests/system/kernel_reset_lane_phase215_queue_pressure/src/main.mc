import boot
import kernel_dispatch
import serial_protocol
import service_effect
import service_topology
import syscall

func cmd(payload: [4]u8) syscall.ReceiveObservation {
    return syscall.ReceiveObservation{ status: syscall.SyscallStatus.Ok, block_reason: syscall.BlockReason.None, endpoint_id: service_topology.SERIAL_ENDPOINT_ID, source_pid: 1, payload_len: 4, received_handle_slot: 0, received_handle_count: 0, payload: payload }
}

func status_is(effect: service_effect.Effect, status: syscall.SyscallStatus) bool {
    if service_effect.effect_reply_status(effect) != status {
        return false
    }
    return true
}

func count_is(effect: service_effect.Effect, count: usize) bool {
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != count {
        return false
    }
    return true
}

func byte_reply_is(effect: service_effect.Effect, value: u8) bool {
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

func smoke_queue_full_rejects_enqueue() bool {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_enqueue(21)))
    if !status_is(effect, syscall.SyscallStatus.Ok) {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_enqueue(22)))
    if !status_is(effect, syscall.SyscallStatus.Ok) {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_enqueue(23)))
    if !status_is(effect, syscall.SyscallStatus.Ok) {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_enqueue(24)))
    if !status_is(effect, syscall.SyscallStatus.Ok) {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_enqueue(99)))
    if !status_is(effect, syscall.SyscallStatus.Exhausted) {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_count()))
    if !count_is(effect, 4) {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_peek()))
    if !byte_reply_is(effect, 21) {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_dequeue()))
    if !byte_reply_is(effect, 21) {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_count()))
    if !count_is(effect, 3) {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_enqueue(42)))
    if !status_is(effect, syscall.SyscallStatus.Ok) {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_count()))
    if !count_is(effect, 4) {
        return false
    }

    return true
}

func main() i32 {
    if !smoke_queue_full_rejects_enqueue() {
        return 1
    }
    return 0
}
