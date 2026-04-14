// Phase 155 temporal semantics and backpressure audit.
//
// This fixture proves the current ordering and backpressure contracts:
//
// Ordering guarantee (Phase 155):
//   Messages dispatched via kernel_dispatch_step are processed in the order
//   they are submitted.  Each step completes before the next begins.  There
//   is no concurrency, pipelining, or out-of-order execution.
//
// Backpressure (Phase 155):
//   Fixed-capacity services (log: 4, kv: 4 keys) return Exhausted when full.
//   The caller receives the Exhausted reply explicitly on the same step that
//   would overflow the buffer.  The service state is unchanged on Exhausted.
//
// Accidental-current-behavior note:
//   The kv-write log marker is advisory and its Exhausted reply is discarded
//   by the dispatcher.  The kv reply is what the caller sees.

import boot
import kernel_dispatch
import ipc
import kv_service
import log_service
import service_effect
import syscall

const SERIAL_ENDPOINT_ID: u32 = 10

func make_obs(b0: u8, b1: u8, b2: u8, b3: u8) syscall.ReceiveObservation {
    payload: [4]u8 = ipc.zero_payload()
    payload[0] = b0
    payload[1] = b1
    payload[2] = b2
    payload[3] = b3
    return syscall.ReceiveObservation{ status: syscall.SyscallStatus.Ok, block_reason: syscall.BlockReason.None, endpoint_id: SERIAL_ENDPOINT_ID, source_pid: 99, payload_len: 4, received_handle_slot: 0, received_handle_count: 0, payload: payload }
}

// Ordering: replies arrive in submission order.
// Three log appends followed by a tail must return the bytes in append order.
func smoke_ordering_is_fifo() bool {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect

    // LA <65> !  — append 'A'
    effect = kernel_dispatch.kernel_dispatch_step(&state, make_obs(76, 65, 65, 33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    // LA <66> !  — append 'B'
    effect = kernel_dispatch.kernel_dispatch_step(&state, make_obs(76, 65, 66, 33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    // LA <67> !  — append 'C'
    effect = kernel_dispatch.kernel_dispatch_step(&state, make_obs(76, 65, 67, 33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    // LT ! !  — tail
    effect = kernel_dispatch.kernel_dispatch_step(&state, make_obs(76, 84, 33, 33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 3 {
        return false
    }
    payload: [4]u8 = service_effect.effect_reply_payload(effect)
    if payload[0] != 65 || payload[1] != 66 || payload[2] != 67 {
        return false
    }
    return true
}

// Backpressure: log buffer full returns Exhausted; state is unchanged.
func smoke_log_full_returns_exhausted() bool {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect

    // Fill all 4 slots.
    effect = kernel_dispatch.kernel_dispatch_step(&state, make_obs(76, 65, 1, 33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, make_obs(76, 65, 2, 33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, make_obs(76, 65, 3, 33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, make_obs(76, 65, 4, 33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    // One more append: must return Exhausted.
    effect = kernel_dispatch.kernel_dispatch_step(&state, make_obs(76, 65, 5, 33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Exhausted {
        return false
    }

    // State must not have grown: tail still returns 4 entries, not 5.
    if log_service.log_len(state.log_state) != 4 {
        return false
    }
    return true
}

// Backpressure: kv store full returns Exhausted for a new key; overwrite is Ok.
func smoke_kv_full_new_key_returns_exhausted() bool {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect

    // Fill 4 distinct keys.  KS <key> <val>.
    effect = kernel_dispatch.kernel_dispatch_step(&state, make_obs(75, 83, 1, 10))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, make_obs(75, 83, 2, 20))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, make_obs(75, 83, 3, 30))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, make_obs(75, 83, 4, 40))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    // New key 5 when full: must return Exhausted.
    effect = kernel_dispatch.kernel_dispatch_step(&state, make_obs(75, 83, 5, 50))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Exhausted {
        return false
    }

    if kv_service.kv_count(state.kv_state) != 4 {
        return false
    }
    return true
}

// Overwriting an existing key when the store is full is always Ok.
func smoke_kv_full_overwrite_is_ok() bool {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect

    effect = kernel_dispatch.kernel_dispatch_step(&state, make_obs(75, 83, 1, 10))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, make_obs(75, 83, 2, 20))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, make_obs(75, 83, 3, 30))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, make_obs(75, 83, 4, 40))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    // Overwrite key 2 when full: must return Ok and update the value.
    effect = kernel_dispatch.kernel_dispatch_step(&state, make_obs(75, 83, 2, 99))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    // Verify the value changed.
    effect = kernel_dispatch.kernel_dispatch_step(&state, make_obs(75, 71, 2, 33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload(effect)[1] != 99 {
        return false
    }
    return true
}

// Delay: a sequential get after a set on the same step chain sees the new value.
// This confirms there is no pipelining or delayed-write buffering.
func smoke_write_then_read_same_chain_is_coherent() bool {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect

    // KS 7 = 11
    effect = kernel_dispatch.kernel_dispatch_step(&state, make_obs(75, 83, 7, 11))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    // KG 7: must see 11, not a stale/default value.
    effect = kernel_dispatch.kernel_dispatch_step(&state, make_obs(75, 71, 7, 33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload(effect)[1] != 11 {
        return false
    }
    return true
}

func main() i32 {
    if !smoke_ordering_is_fifo() {
        return 1
    }
    if !smoke_log_full_returns_exhausted() {
        return 2
    }
    if !smoke_kv_full_new_key_returns_exhausted() {
        return 3
    }
    if !smoke_kv_full_overwrite_is_ok() {
        return 4
    }
    if !smoke_write_then_read_same_chain_is_coherent() {
        return 5
    }
    return 0
}
