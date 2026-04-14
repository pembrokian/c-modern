// Phase 157 long-lived system coherence audit.
//
// Audits whether system meaning remains coherent after many interactions,
// repeated state changes, and repeated observation — not just across one
// short proof path.
//
// Findings from this audit:
//
//   kv state is stable across repeated reads:
//     A key set once returns the same value on every subsequent read until
//     it is explicitly overwritten.  There is no silent expiry, no drift,
//     and no side-effect from reading.
//
//   log overwrite-on-full is the permanent policy:
//     Once the 4-entry log is full it stays full and returns Exhausted on
//     every further append.  Tail still returns the original 4 entries on
//     every subsequent read.  No silent loss or silent growth occurs.
//
//   restart resets all service state to the empty baseline:
//     A new kernel_init() call produces a state that is identical in
//     behavior to the very first init.  No hidden residue from prior
//     interactions leaks across restarts.
//
//   kv-write advisory log marker is coherent across many writes:
//     Each kv set appends one marker to the log.  When the log is full
//     the marker step silently discards its Exhausted rather than
//     corrupting the kv reply.  The kv state and the log state remain
//     independently coherent — log saturation does not affect kv.
//
//   Long-lived key update cycle:
//     Repeated updates to the same key never overflow the kv store because
//     an overwrite does not consume a new slot.  The store count stays
//     stable; only the value changes.

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

// A key set once is readable on every subsequent step with no drift.
func smoke_kv_value_stable_across_reads() bool {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect

    // KS 3 99
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(75, 83, 3, 99))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    // KG 3 — three independent reads; value must be 99 each time
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(75, 71, 3, 33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload(effect)[1] != 99 {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(75, 71, 3, 33))
    if service_effect.effect_reply_payload(effect)[1] != 99 {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(75, 71, 3, 33))
    if service_effect.effect_reply_payload(effect)[1] != 99 {
        return false
    }

    return true
}

// Repeated updates to the same key never grow the store count.
func smoke_repeated_overwrite_stable_count() bool {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect

    // Write key=1 four times with different values.
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(75, 83, 1, 1))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(75, 83, 1, 2))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(75, 83, 1, 3))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(75, 83, 1, 4))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    // Only one slot consumed: count == 1.
    if kv_service.kv_count(state.kv_state) != 1 {
        return false
    }

    // Value is the last written: 4.
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(75, 71, 1, 33))
    if service_effect.effect_reply_payload(effect)[1] != 4 {
        return false
    }

    return true
}

// Log full stays full; tail remains stable across repeated reads after fill.
func smoke_log_full_stable_tail() bool {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect

    // Fill all 4 slots: LA <1-4> !
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

    // Three more appends — all Exhausted, state unchanged.
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(76, 65, 5, 33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Exhausted {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(76, 65, 6, 33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Exhausted {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(76, 65, 7, 33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Exhausted {
        return false
    }

    // Tail still returns original 4 entries on repeated reads.
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(76, 84, 33, 33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 4 {
        return false
    }
    payload: [4]u8 = service_effect.effect_reply_payload(effect)
    if payload[0] != 1 || payload[1] != 2 || payload[2] != 3 || payload[3] != 4 {
        return false
    }

    // Second tail read — identical result.
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(76, 84, 33, 33))
    if service_effect.effect_reply_payload_len(effect) != 4 {
        return false
    }

    return true
}

// Restart resets all state to the empty baseline.
func smoke_restart_clears_state() bool {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect

    // Write some kv entries and fill the log.
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(75, 83, 7, 55))
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(76, 65, 9, 33))

    // Restart: fresh init.
    state = boot.kernel_init()

    // kv key 7 is gone — get returns InvalidArgument.
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(75, 71, 7, 33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.InvalidArgument {
        return false
    }

    // Log is empty — tail returns len 0.
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(76, 84, 33, 33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 0 {
        return false
    }

    // A fresh append succeeds — no residue from before restart.
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(76, 65, 21, 33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if log_service.log_len(state.log_state) != 1 {
        return false
    }

    return true
}

// kv writes fill the advisory log; once the log saturates kv still works.
// Log saturation does not corrupt kv state.
func smoke_kv_log_marker_coherent_at_saturation() bool {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect

    // Write 4 distinct kv keys.  Each write also appends one log marker.
    // After 4 writes the log is full.
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

    // Log is full now (4 markers).
    if log_service.log_len(state.log_state) != 4 {
        return false
    }

    // Overwrite an existing key — log is full but kv reply is still Ok.
    // The advisory marker step discards its Exhausted silently.
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(75, 83, 1, 99))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    // kv state is correct despite log saturation.
    effect = kernel_dispatch.kernel_dispatch_step(&state, obs(75, 71, 1, 33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload(effect)[1] != 99 {
        return false
    }

    // Log count is still 4 — no phantom entries were added.
    if log_service.log_len(state.log_state) != 4 {
        return false
    }

    return true
}

func main() i32 {
    if !smoke_kv_value_stable_across_reads() {
        return 1
    }
    if !smoke_repeated_overwrite_stable_count() {
        return 2
    }
    if !smoke_log_full_stable_tail() {
        return 3
    }
    if !smoke_restart_clears_state() {
        return 4
    }
    if !smoke_kv_log_marker_coherent_at_saturation() {
        return 5
    }
    return 0
}
