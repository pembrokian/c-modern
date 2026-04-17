// Phase 216 stall consequence fixture.
//
// Proves the queue service stall consequence model:
//   - first push to a full queue returns Exhausted (stall_count 0 → 1)
//   - second push to a full queue returns WouldBlock (stall_count > 0)
//   - the stall count is queryable via Q W ! !
//   - a successful push after draining resets the stall count to zero

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

func stall_count_is(effect: service_effect.Effect, count: usize) bool {
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != count {
        return false
    }
    return true
}

func smoke_stall_consequence() bool {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect

    // Fill the queue to capacity.
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_enqueue(10)))
    if !status_is(effect, syscall.SyscallStatus.Ok) {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_enqueue(11)))
    if !status_is(effect, syscall.SyscallStatus.Ok) {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_enqueue(12)))
    if !status_is(effect, syscall.SyscallStatus.Ok) {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_enqueue(13)))
    if !status_is(effect, syscall.SyscallStatus.Ok) {
        return false
    }

    // Stall count starts at zero.
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_stall_count()))
    if !stall_count_is(effect, 0) {
        return false
    }

    // First overflow: Exhausted, stall_count becomes 1.
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_enqueue(99)))
    if !status_is(effect, syscall.SyscallStatus.Exhausted) {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_stall_count()))
    if !stall_count_is(effect, 1) {
        return false
    }

    // Second overflow: WouldBlock (persistent stall consequence), stall_count becomes 2.
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_enqueue(99)))
    if !status_is(effect, syscall.SyscallStatus.WouldBlock) {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_stall_count()))
    if !stall_count_is(effect, 2) {
        return false
    }

    // Drain one item to create space.
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_dequeue()))
    if !status_is(effect, syscall.SyscallStatus.Ok) {
        return false
    }
    if service_effect.effect_reply_payload(effect)[0] != 10 {
        return false
    }

    // Successful push resets stall_count to zero.
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_enqueue(50)))
    if !status_is(effect, syscall.SyscallStatus.Ok) {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_queue_stall_count()))
    if !stall_count_is(effect, 0) {
        return false
    }

    return true
}

func main() i32 {
    if !smoke_stall_consequence() {
        return 1
    }
    return 0
}
