// Phase 159 observability as a system property.
//
// Classification of the minimum user-meaningful observation surface:
//
//   System-visible truth (user can rely on these via the public protocol):
//     - kv_count (KC!!) — how many entries are stored; Ok reply, count in payload_len
//     - kv_get (KG <key> !) — value for a known key; Ok on hit, InvalidArgument on miss
//     - log_tail (LT!!) — all retained log entries; Ok reply, entries in payload
//
//   Implementation detail (not a user contract):
//     - KV_WRITE_LOG_MARKER byte value (advisory; may be silently dropped)
//     - endpoint IDs (boot-wired constants; callers use the serial endpoint only)
//     - internal kv_service.kv_count() accessor (available inside the service only)
//
// This fixture audits one debugging workflow:
//   1. Write some state.
//   2. Use kv_count to determine how many entries exist without knowing the keys.
//   3. Use log_tail to read all retained observations.
//   4. Verify observation is non-mutating (reads do not change state).
//
// Before Phase 159, a user could not determine kv occupancy via the public
// protocol.  The count query closes that gap with zero new module surface.

import boot
import ipc
import kv_service
import log_service
import serial_protocol
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

// kv_count returns Ok with the entry count in reply_payload_len.
// An empty store returns count=0; writes increment it up to KV_CAPACITY.
func smoke_kv_count_reflects_writes() bool {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect

    // Empty store: count is 0.
    effect = boot.kernel_dispatch_step(&state, obs(75, 67, 33, 33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 0 {
        return false
    }

    // KS <key=1> <val=10>: one entry.
    effect = boot.kernel_dispatch_step(&state, obs(75, 83, 1, 10))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    // Count is now 1.
    effect = boot.kernel_dispatch_step(&state, obs(75, 67, 33, 33))
    if service_effect.effect_reply_payload_len(effect) != 1 {
        return false
    }

    // KS <key=2> <val=20>: two distinct entries.
    effect = boot.kernel_dispatch_step(&state, obs(75, 83, 2, 20))
    effect = boot.kernel_dispatch_step(&state, obs(75, 67, 33, 33))
    if service_effect.effect_reply_payload_len(effect) != 2 {
        return false
    }

    // Overwrite key=1: count stays at 2.
    effect = boot.kernel_dispatch_step(&state, obs(75, 83, 1, 99))
    effect = boot.kernel_dispatch_step(&state, obs(75, 67, 33, 33))
    if service_effect.effect_reply_payload_len(effect) != 2 {
        return false
    }

    return true
}

// Observation is non-mutating: reading kv count twice returns the same value.
func smoke_observation_is_non_mutating() bool {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect

    // Write one entry.
    effect = boot.kernel_dispatch_step(&state, obs(75, 83, 5, 42))

    // Count twice: both should return 1.
    effect = boot.kernel_dispatch_step(&state, obs(75, 67, 33, 33))
    if service_effect.effect_reply_payload_len(effect) != 1 {
        return false
    }
    effect = boot.kernel_dispatch_step(&state, obs(75, 67, 33, 33))
    if service_effect.effect_reply_payload_len(effect) != 1 {
        return false
    }

    return true
}

// log_tail is always readable: observation never triggers backpressure.
// Exhausted is only for writes; a full log still serves reads with Ok.
func smoke_log_tail_readable_at_capacity() bool {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect

    // Fill log to capacity with LA <value> !.
    effect = boot.kernel_dispatch_step(&state, obs(76, 65, 10, 33))
    effect = boot.kernel_dispatch_step(&state, obs(76, 65, 20, 33))
    effect = boot.kernel_dispatch_step(&state, obs(76, 65, 30, 33))
    effect = boot.kernel_dispatch_step(&state, obs(76, 65, 40, 33))

    // Log is full: a write returns Exhausted.
    effect = boot.kernel_dispatch_step(&state, obs(76, 65, 50, 33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Exhausted {
        return false
    }

    // LT!! returns Ok at full capacity; all 4 entries are visible.
    effect = boot.kernel_dispatch_step(&state, obs(76, 84, 33, 33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 4 {
        return false
    }
    if service_effect.effect_reply_payload(effect)[0] != 10 {
        return false
    }
    if service_effect.effect_reply_payload(effect)[3] != 40 {
        return false
    }

    // Reading log a second time: still Ok and still 4 entries (non-mutating).
    effect = boot.kernel_dispatch_step(&state, obs(76, 84, 33, 33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 4 {
        return false
    }

    return true
}

// kv_count at capacity returns KV_CAPACITY (4), not Exhausted.
// Capacity overflow is only signaled on writes.
func smoke_kv_count_at_capacity_is_ok() bool {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect

    effect = boot.kernel_dispatch_step(&state, obs(75, 83, 1, 10))
    effect = boot.kernel_dispatch_step(&state, obs(75, 83, 2, 20))
    effect = boot.kernel_dispatch_step(&state, obs(75, 83, 3, 30))
    effect = boot.kernel_dispatch_step(&state, obs(75, 83, 4, 40))

    // Store is full: a new key returns Exhausted.
    effect = boot.kernel_dispatch_step(&state, obs(75, 83, 5, 50))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Exhausted {
        return false
    }

    // Count query still returns Ok with count=4.
    effect = boot.kernel_dispatch_step(&state, obs(75, 67, 33, 33))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 4 {
        return false
    }

    return true
}

func main() i32 {
    if smoke_kv_count_reflects_writes() == false {
        return 1
    }
    if smoke_observation_is_non_mutating() == false {
        return 2
    }
    if smoke_log_tail_readable_at_capacity() == false {
        return 3
    }
    if smoke_kv_count_at_capacity_is_ok() == false {
        return 4
    }
    return 0
}
