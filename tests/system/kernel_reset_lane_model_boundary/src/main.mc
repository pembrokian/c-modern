// Phase 160 model boundary identification.
//
// Named boundary: advisory audit silencing is invisible to the kv caller.
//
// When the log is at capacity, a kv write returns Ok but its advisory audit
// marker is silently discarded by the boot dispatcher.  The kv caller has
// no protocol-level signal that the audit trail was lost.  The effect model
// has no witness for advisory send outcomes.
//
// Evidence across Phase 152-159:
//   Phase 152: log restart while kv lives -- audit gap during selective restart
//   Phase 156: shared namespace -- no per-client audit isolation
//   Phase 157: long-lived coherence -- state correct, audit trail diverges
//   Phase 158: explicitly named advisory silencing as the one cross-service
//              inconsistency
//   Phase 159: kv count observable, but count-vs-log-len correlation breaks
//              once the log is full
//
// Narrowest next expansion axis (named, not admitted):
//   Per-send delivery witnesses.  A witness would let the dispatcher surface
//   Dropped to the kv caller when the advisory send was not delivered.
//   No witness implementation admitted in this phase.
//
// Proof structure:
//   smoke_audit_silencing_is_invisible:
//     Fill the log via LT!! query to confirm capacity.  Write two kv entries.
//     Verify both returned Ok.  Read log tail again: still 4 entries.
//     Read kv count: grew by 2.
//     Both sides observed through the public protocol only.
//
//   smoke_no_protocol_witness_for_advisory_send:
//     Case A: log has space -- advisory marker is delivered to the log.
//     Case B: log is full  -- advisory marker is silently dropped.
//     Assert both kv-write replies are identical (same status, same payload).
//     This is the indistinguishability proof -- the caller cannot tell them apart.

import boot
import kernel_dispatch
import serial_protocol
import service_effect
import syscall

// Log capacity is 4.  log_service returns Exhausted on append when full and
// does not overwrite retained entries.  The advisory kv-write marker is
// silently dropped by the dispatcher when the log is full -- the Exhausted
// reply from the advisory send is discarded before it reaches the kv caller.
const SERIAL_ENDPOINT_ID: u32 = 10

func cmd(payload: [4]u8) syscall.ReceiveObservation {
    return syscall.ReceiveObservation{ status: syscall.SyscallStatus.Ok, block_reason: syscall.BlockReason.None, endpoint_id: SERIAL_ENDPOINT_ID, source_pid: 1, payload_len: 4, received_handle_slot: 0, received_handle_count: 0, payload: payload }
}

// The kv caller receives Ok for writes that had no advisory audit record.
// The log count stays at capacity; the kv count grows.  The gap is silent.
// Both sides are observed purely through the public protocol.
func smoke_audit_silencing_is_invisible() bool {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect

    // Fill the log to capacity via direct log-append commands.
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_log_append(1)))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_log_append(2)))
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_log_append(3)))
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_log_append(4)))

    // Confirm log is full via protocol: tail returns 4 entries.
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_log_tail()))
    if service_effect.effect_reply_payload_len(effect) != 4 {
        return false
    }

    // Write one kv entry.  Returns Ok even though advisory marker cannot land.
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_kv_set(10, 55)))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    // Write a second kv entry.  Again Ok; again the marker is dropped.
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_kv_set(11, 66)))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    // Log length is still 4 via protocol: both advisory markers were dropped.
    // This is the named boundary.
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_log_tail()))
    if service_effect.effect_reply_payload_len(effect) != 4 {
        return false
    }

    // kv count grew by 2: both entries were stored correctly.
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_kv_count()))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 2 {
        return false
    }

    return true
}

// A kv-write Ok reply is identical whether the advisory audit marker was
// delivered or silently dropped.  The caller cannot distinguish the two cases.
//
// Case A: log has space -- advisory marker IS delivered to the log.
// Case B: log is full  -- advisory marker is silently dropped.
// Assert the replies are identical.  That is the indistinguishability proof.
func smoke_no_protocol_witness_for_advisory_send() bool {
    // Case A: fresh state, log is empty, advisory kv-write marker can land.
    state_a: boot.KernelBootState = boot.kernel_init()
    effect_a: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(&state_a, cmd(serial_protocol.encode_kv_set(5, 99)))

    // Case B: log is full, advisory kv-write marker is silently dropped.
    state_b: boot.KernelBootState = boot.kernel_init()
    effect_b: service_effect.Effect
    effect_b = kernel_dispatch.kernel_dispatch_step(&state_b, cmd(serial_protocol.encode_log_append(10)))
    effect_b = kernel_dispatch.kernel_dispatch_step(&state_b, cmd(serial_protocol.encode_log_append(20)))
    effect_b = kernel_dispatch.kernel_dispatch_step(&state_b, cmd(serial_protocol.encode_log_append(30)))
    effect_b = kernel_dispatch.kernel_dispatch_step(&state_b, cmd(serial_protocol.encode_log_append(40)))
    effect_b = kernel_dispatch.kernel_dispatch_step(&state_b, cmd(serial_protocol.encode_kv_set(5, 99)))

    // Both replies carry the same status and payload: indistinguishable.
    if service_effect.effect_reply_status(effect_a) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_status(effect_b) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload_len(effect_a) != 0 {
        return false
    }
    if service_effect.effect_reply_payload_len(effect_b) != 0 {
        return false
    }
    if service_effect.effect_reply_status(effect_a) != service_effect.effect_reply_status(effect_b) {
        return false
    }
    if service_effect.effect_reply_payload_len(effect_a) != service_effect.effect_reply_payload_len(effect_b) {
        return false
    }

    return true
}

func main() i32 {
    if smoke_audit_silencing_is_invisible() == false {
        return 1
    }
    if smoke_no_protocol_witness_for_advisory_send() == false {
        return 2
    }
    return 0
}
