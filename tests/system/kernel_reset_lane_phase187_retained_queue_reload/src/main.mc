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

func queue_value_is(effect: service_effect.Effect, value: u8) bool {
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

func smoke_queue_restart_reloads_order() bool {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_enqueue(10)))
    if !queue_status_is(effect, syscall.SyscallStatus.Ok) {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_enqueue(20)))
    if !queue_status_is(effect, syscall.SyscallStatus.Ok) {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_enqueue(30)))
    if !queue_status_is(effect, syscall.SyscallStatus.Ok) {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_enqueue(40)))
    if !queue_status_is(effect, syscall.SyscallStatus.Ok) {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_enqueue(50)))
    if !queue_status_is(effect, syscall.SyscallStatus.Exhausted) {
        return false
    }

    state = init.restart(state, service_topology.QUEUE_ENDPOINT_ID)

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_dequeue()))
    if !queue_value_is(effect, 10) {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_dequeue()))
    if !queue_value_is(effect, 20) {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_dequeue()))
    if !queue_value_is(effect, 30) {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_dequeue()))
    if !queue_value_is(effect, 40) {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_dequeue()))
    if !queue_status_is(effect, syscall.SyscallStatus.InvalidArgument) {
        return false
    }

    return true
}

func main() i32 {
    if !smoke_queue_restart_reloads_order() {
        return 1
    }
    return 0
}