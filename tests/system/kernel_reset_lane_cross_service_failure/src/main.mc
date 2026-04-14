// Phase 158 cross-service failure semantics consistency audit.
//
// This fixture proves which cross-service failure differences are intentional
// and which are consistent across the admitted service set.
//
// Findings from this audit:
//
//   Advisory log drop is intentional (cross-service difference):
//     When the log buffer is at capacity, a kv write still returns Ok.
//     The dispatcher silently discards the advisory log marker for that write.
//     The kv caller sees no signal that the audit trail was lost.
//     This is the one named cross-service failure inconsistency: kv success
//     does not imply that the associated log record was written.
//
//   InvalidArgument is consistent (aligned):
//     kv get for a missing key and a shell invalid command both return
//     InvalidArgument.  The same status code signals "request cannot be
//     fulfilled as stated" across these two service boundaries.
//
//   Exhausted is consistent (aligned):
//     Log full and kv full both return Exhausted with no state change.
//     The caller gets the same status code from both services when a
//     fixed-capacity boundary is reached.
//
//   Exhausted is sticky (aligned):
//     A full log stays full regardless of kv activity.  A kv write does
//     not consume or free log capacity.  Services own their capacity
//     independently; one service reaching its limit does not affect another.

import boot
import kernel_dispatch
import ipc
import kv_service
import log_service
import service_effect
import syscall

const SERIAL_ENDPOINT_ID: u32 = 10

func obs(b0: u8, b1: u8, b2: u8, b3: u8) syscall.ReceiveObservation {
    payload: [4]u8 = ipc.zero_payload()
    payload[0] = b0
    payload[1] = b1
    payload[2] = b2
    payload[3] = b3
    return syscall.ReceiveObservation{ status: syscall.SyscallStatus.Ok, block_reason: syscall.BlockReason.None, endpoint_id: SERIAL_ENDPOINT_ID, source_pid: 1, payload_len: 4, received_handle_slot: 0, received_handle_count: 0, payload: payload }
}

// Advisory log drop: a kv write returns Ok even when the log buffer is full.
// The caller has no signal that the advisory log record was silently lost.
func smoke_kv_write_ok_masks_advisory_log_drop() bool {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect

    // Fill the log to capacity with four appends: LA <1> !, LA <2> !, LA <3> !, LA <4> !
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(76, 65, 1, 33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(76, 65, 2, 33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(76, 65, 3, 33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(76, 65, 4, 33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    // Log is now at capacity.  Confirm it returns Exhausted.
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(76, 65, 5, 33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Exhausted {
        return false
    }

    // kv write with a new key: the kv operation succeeds.
    // The advisory log marker attempt is silently discarded by the dispatcher.
    // KS <key=7> <val=42>
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(75, 83, 7, 42))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    // Log is still full; the kv write freed no log capacity.
    if log_service.log_len(state.log_state) != 4 {
        return false
    }

    // Another log append still returns Exhausted.
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(76, 65, 6, 33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Exhausted {
        return false
    }

    return true
}

// Exhausted is consistent across services: log full and kv full both return
// Exhausted with no state change.  The same status code signals capacity
// overflow regardless of which service owns the buffer.
func smoke_exhausted_is_consistent_across_services() bool {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect

    // Fill log to 4 entries.
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(76, 65, 10, 33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(76, 65, 11, 33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(76, 65, 12, 33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(76, 65, 13, 33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    // Fifth log append: Exhausted.
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(76, 65, 14, 33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Exhausted {
        return false
    }

    // Fill kv to 4 distinct keys.  KS <key> <val>.
    // Note: each kv write also tries the advisory log but log is already full;
    // the advisory is silently dropped; kv replies are Ok.
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(75, 83, 1, 10))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(75, 83, 2, 20))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(75, 83, 3, 30))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(75, 83, 4, 40))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    // Fifth kv write with a new key: Exhausted — same status as log full.
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(75, 83, 5, 50))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Exhausted {
        return false
    }

    // kv count is still 4; state is unchanged on Exhausted.
    if kv_service.kv_count(state.kv_state) != 4 {
        return false
    }

    return true
}

// InvalidArgument is consistent: kv get for a missing key and a shell invalid
// command both return InvalidArgument.  The same status code means "this
// request cannot be fulfilled as stated" across both service boundaries.
func smoke_invalid_argument_is_consistent_across_services() bool {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect

    // kv get for a key that was never set: InvalidArgument.
    // KG <key=99> !
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(75, 71, 99, 33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.InvalidArgument {
        return false
    }

    // Shell invalid command (bytes that match no known protocol pattern): InvalidArgument.
    // [0, 0, 0, 0] matches none of EC, LA, LT, KS, KG patterns.
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(0, 0, 0, 0))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.InvalidArgument {
        return false
    }

    return true
}

// Advisory log drop for overwrite: a kv overwrite also returns Ok when the
// log is full, with the same silent drop semantics as a new-key write.
// The caller cannot distinguish "write ok, log recorded" from "write ok,
// log dropped" by examining the reply status alone.
func smoke_kv_overwrite_ok_masks_advisory_log_drop() bool {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect

    // Write key 6 = 10.
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(75, 83, 6, 10))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    // Fill log to capacity.
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(76, 65, 1, 33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(76, 65, 2, 33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(76, 65, 3, 33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    // Log now has: [kv_marker=75, 1, 2, 3] = 4 entries (full).
    if log_service.log_len(state.log_state) != 4 {
        return false
    }

    // Overwrite key 6: kv says Ok; advisory log marker is silently dropped.
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(75, 83, 6, 99))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    // Confirm the overwrite took effect: KG 6 should return 99.
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(75, 71, 6, 33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload(effect)[1] != 99 {
        return false
    }

    return true
}

func main() i32 {
    if !smoke_kv_write_ok_masks_advisory_log_drop() {
        return 1
    }
    if !smoke_exhausted_is_consistent_across_services() {
        return 2
    }
    if !smoke_invalid_argument_is_consistent_across_services() {
        return 3
    }
    if !smoke_kv_overwrite_ok_masks_advisory_log_drop() {
        return 4
    }
    return 0
}
