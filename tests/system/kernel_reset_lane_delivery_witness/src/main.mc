// Phase 161 per-send delivery witness.
//
// Proves that the Effect model now carries a send_dropped witness for
// advisory kv-write log appends.
//
// Case A: log has space.
//   A kv write triggers an advisory log append that succeeds.
//   effect_send_dropped() returns 0.  The caller knows the audit landed.
//
// Case B: log is full.
//   A kv write triggers an advisory log append that is Exhausted.
//   effect_send_dropped() returns 1.  The caller knows the audit was dropped.
//
// Both cases return Ok as the primary reply status: the witness is orthogonal
// to success/failure of the kv operation itself.
//
// This closes the boundary named in Phase 160: the caller can now
// distinguish advisory-delivered from advisory-dropped using the public
// Effect accessor.

import boot
import kernel_dispatch
import serial_protocol
import service_effect
import syscall

const SERIAL_ENDPOINT_ID: u32 = 10

func cmd(payload: [4]u8) syscall.ReceiveObservation {
    return syscall.ReceiveObservation{ status: syscall.SyscallStatus.Ok, block_reason: syscall.BlockReason.None, endpoint_id: SERIAL_ENDPOINT_ID, source_pid: 1, payload_len: 4, received_handle_slot: 0, received_handle_count: 0, payload: payload }
}

// Case A: fresh state, log is empty.
// kv write → advisory log append succeeds → send_dropped = 0.
func smoke_witness_absent_when_delivered() bool {
    state: boot.KernelBootState = boot.kernel_init()

    effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_kv_set(1, 10)))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    // Advisory send succeeded: witness is 0.
    if service_effect.effect_send_dropped(effect) != 0 {
        return false
    }

    return true
}

// Case B: log is full.
// kv write → advisory log append is Exhausted → send_dropped = 1.
func smoke_witness_present_when_dropped() bool {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect

    // Fill the log to capacity (4 entries).
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_log_append(1)))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_log_append(2)))
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_log_append(3)))
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_log_append(4)))

    // Confirm log is full.
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_log_tail()))
    if service_effect.effect_reply_payload_len(effect) != 4 {
        return false
    }

    // kv write with log full: advisory append is Exhausted.
    // Primary reply must still be Ok.
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_kv_set(5, 99)))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    // Advisory send was dropped: witness is 1.
    if service_effect.effect_send_dropped(effect) != 1 {
        return false
    }

    return true
}

// Indistinguishability is now broken: the two cases produce different
// send_dropped values.  A caller CAN tell them apart.
func smoke_caller_can_distinguish_cases() bool {
    // Case A: fresh state, log empty.
    state_a: boot.KernelBootState = boot.kernel_init()
    effect_a: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(&state_a, cmd(serial_protocol.encode_kv_set(5, 99)))

    // Case B: log full.
    state_b: boot.KernelBootState = boot.kernel_init()
    effect_b: service_effect.Effect
    effect_b = kernel_dispatch.kernel_dispatch_step(&state_b, cmd(serial_protocol.encode_log_append(10)))
    effect_b = kernel_dispatch.kernel_dispatch_step(&state_b, cmd(serial_protocol.encode_log_append(20)))
    effect_b = kernel_dispatch.kernel_dispatch_step(&state_b, cmd(serial_protocol.encode_log_append(30)))
    effect_b = kernel_dispatch.kernel_dispatch_step(&state_b, cmd(serial_protocol.encode_log_append(40)))
    effect_b = kernel_dispatch.kernel_dispatch_step(&state_b, cmd(serial_protocol.encode_kv_set(5, 99)))

    // Both replies are Ok.
    if service_effect.effect_reply_status(effect_a) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_status(effect_b) != syscall.SyscallStatus.Ok {
        return false
    }

    // The witnesses differ: case A delivered, case B dropped.
    if service_effect.effect_send_dropped(effect_a) != 0 {
        return false
    }
    if service_effect.effect_send_dropped(effect_b) != 1 {
        return false
    }

    return true
}

func main() i32 {
    if smoke_witness_absent_when_delivered() == false {
        return 1
    }
    if smoke_witness_present_when_dropped() == false {
        return 2
    }
    if smoke_caller_can_distinguish_cases() == false {
        return 3
    }
    return 0
}
