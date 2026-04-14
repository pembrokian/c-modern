// Integration scenarios over the boot kernel state.
//
// Each function runs one named interaction slice, feeds commands through
// kernel_dispatch_step, and returns 0 on success or a non-zero error code.
// All scenarios chain on the same state passed in by the caller; later
// scenarios can observe state written by earlier ones.
//
// This module also owns the transport adapter layer: serial_obs wraps a
// protocol payload into a syscall.ReceiveObservation.  endpoint and pid are
// explicit parameters so the transport can be swapped without touching the
// protocol encoding in serial_protocol.mc.

import boot
import log_service
import serial_protocol
import service_effect
import syscall

// Bootstrap limitation: cross-module constant references are not yet supported
// at link time, so this const is declared locally rather than via boot.SERIAL_ENDPOINT_ID.
const SERIAL_ENDPOINT_ID: u32 = 10

// serial_obs wraps a protocol payload into a ReceiveObservation.
// endpoint and pid are explicit: swap the endpoint to route to a different
// service; swap pid to simulate a different client.
func serial_obs(endpoint: u32, pid: u32, payload: [4]u8) syscall.ReceiveObservation {
    return syscall.ReceiveObservation{ status: syscall.SyscallStatus.Ok, block_reason: syscall.BlockReason.None, endpoint_id: endpoint, source_pid: pid, payload_len: 4, received_handle_slot: 0, received_handle_count: 0, payload: payload }
}

// cmd_* are single-client convenience wrappers: serial endpoint, pid=1.
func cmd_log_append(value: u8) syscall.ReceiveObservation {
    return serial_obs(SERIAL_ENDPOINT_ID, 1, serial_protocol.encode_log_append(value))
}

func cmd_log_tail() syscall.ReceiveObservation {
    return serial_obs(SERIAL_ENDPOINT_ID, 1, serial_protocol.encode_log_tail())
}

func cmd_kv_set(key: u8, value: u8) syscall.ReceiveObservation {
    return serial_obs(SERIAL_ENDPOINT_ID, 1, serial_protocol.encode_kv_set(key, value))
}

func cmd_kv_get(key: u8) syscall.ReceiveObservation {
    return serial_obs(SERIAL_ENDPOINT_ID, 1, serial_protocol.encode_kv_get(key))
}

// Basic single-client kv and log round-trip.
func run_basic_kv_log(state: *boot.KernelBootState) i32 {
    effect: service_effect.Effect

    effect = boot.kernel_dispatch_step(state, cmd_log_append(77))
    if boot.debug_boot_routed(effect) == 0 {
        return 1
    }

    effect = boot.kernel_dispatch_step(state, cmd_kv_set(5, 42))
    if boot.debug_boot_routed(effect) == 0 {
        return 1
    }

    effect = boot.kernel_dispatch_step(state, cmd_kv_get(5))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return 1
    }
    if service_effect.effect_reply_payload(effect)[1] != 42 {
        return 1
    }

    effect = boot.kernel_dispatch_step(state, cmd_log_tail())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return 1
    }
    s: boot.KernelBootState = *state
    if service_effect.effect_reply_payload_len(effect) != log_service.log_len(s.log_state) {
        return 1
    }
    if service_effect.effect_reply_payload(effect)[0] != 77 {
        return 1
    }
    if service_effect.effect_reply_payload(effect)[1] != 75 {
        return 1
    }

    return 0
}

// Multi-client: a second source_pid reads state written by the first.
// Shared state; no per-client namespace; reply is the return value of the step.
func run_multi_client(state: *boot.KernelBootState) i32 {
    effect: service_effect.Effect

    effect = boot.kernel_dispatch_step(state, serial_obs(SERIAL_ENDPOINT_ID, 99, serial_protocol.encode_kv_get(5)))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return 1
    }
    if service_effect.effect_reply_payload(effect)[1] != 42 {
        return 1
    }

    return 0
}

// Long-lived coherence: repeated overwrites and repeated reads stay consistent.
func run_long_lived_coherence(state: *boot.KernelBootState) i32 {
    effect: service_effect.Effect

    effect = boot.kernel_dispatch_step(state, cmd_kv_set(5, 77))
    if boot.debug_boot_routed(effect) == 0 {
        return 1
    }

    effect = boot.kernel_dispatch_step(state, cmd_kv_set(5, 99))
    if boot.debug_boot_routed(effect) == 0 {
        return 1
    }

    effect = boot.kernel_dispatch_step(state, cmd_kv_get(5))
    if service_effect.effect_reply_payload(effect)[1] != 99 {
        return 1
    }

    effect = boot.kernel_dispatch_step(state, cmd_kv_get(5))
    if service_effect.effect_reply_payload(effect)[1] != 99 {
        return 1
    }

    return 0
}

// Cross-service failure semantics (Phase 158).
//
// Called after run_long_lived_coherence; the log is at capacity (4 entries)
// at this point due to the kv-write advisory markers written by prior scenarios.
//
// This scenario classifies three cross-service failure findings:
//
//   Advisory log drop (intentional difference):
//     When the log is full, a kv write still returns Ok.  The advisory log
//     marker is silently discarded by the dispatcher.  The kv caller has no
//     visible signal that the audit trail was lost.  This is the one
//     cross-service failure inconsistency: kv success does not imply that
//     the associated log record was written.
//
//   Aligned InvalidArgument (consistent):
//     kv get for a missing key and a shell invalid command both return
//     InvalidArgument.  The same status code signals "request cannot be
//     fulfilled as stated" across service boundaries.
//
//   Exhausted state is sticky (consistent):
//     Log full returns Exhausted on every further append.  A kv write does
//     not free log capacity.  The exhausted state persists until restart.
func run_cross_service_failure(state: *boot.KernelBootState) i32 {
    effect: service_effect.Effect

    // Log is full from prior scenarios.  Any further append must return Exhausted.
    effect = boot.kernel_dispatch_step(state, cmd_log_append(42))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Exhausted {
        return 1
    }

    // kv write with a new key: the kv operation succeeds (Ok) even though the
    // advisory log marker is silently dropped.  The caller sees Ok and has no
    // signal that the log record was lost.
    effect = boot.kernel_dispatch_step(state, cmd_kv_set(20, 55))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return 2
    }

    // Log is still full; the kv write did not free any log capacity.
    effect = boot.kernel_dispatch_step(state, cmd_log_append(1))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Exhausted {
        return 3
    }

    // kv get for a key that was never set returns InvalidArgument.
    // This is the consistent cross-service signal for an unfulfillable request.
    effect = boot.kernel_dispatch_step(state, cmd_kv_get(99))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.InvalidArgument {
        return 4
    }

    // kv overwrite also succeeds when the log is full: advisory marker is
    // dropped for overwrites as well, with the same silent loss semantics.
    effect = boot.kernel_dispatch_step(state, cmd_kv_set(20, 77))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return 5
    }

    return 0
}

// Observability inspection (Phase 159).
//
// Called after run_cross_service_failure.
// State: kv has count=2 (keys 5 and 20), log is at capacity (4 retained entries).
// run_model_boundary (Phase 160) is called after this and expects that same state.
//
// Proves the user can inspect system state truthfully via the public protocol
// without requiring internal knowledge:
//
//   kv_count (KC!!) returns how many entries exist without the caller needing
//   to know the stored keys.  This is the minimum gap Phase 159 closes: the
//   log was already fully observable via LT!!, but kv state was opaque unless
//   the caller already knew the keys.
//
//   log_tail (LT!!) returns all retained entries even when the buffer is full.
//   Observation never triggers backpressure; Exhausted is only for writes.
//
//   Observation is non-mutating: a second count query returns the same value.
func run_observability_inspection(state: *boot.KernelBootState) i32 {
    effect: service_effect.Effect

    // kv count query: proves a user can determine how many entries exist
    // without needing to know the stored keys in advance.
    effect = boot.kernel_dispatch_step(state, serial_obs(SERIAL_ENDPOINT_ID, 1, serial_protocol.encode_kv_count()))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return 1
    }
    if service_effect.effect_reply_payload_len(effect) != 2 {
        return 2
    }

    // Observation is non-mutating: a second count returns the same value.
    effect = boot.kernel_dispatch_step(state, serial_obs(SERIAL_ENDPOINT_ID, 1, serial_protocol.encode_kv_count()))
    if service_effect.effect_reply_payload_len(effect) != 2 {
        return 3
    }

    // Log tail returns all retained entries at capacity: observation never
    // triggers backpressure.
    effect = boot.kernel_dispatch_step(state, cmd_log_tail())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return 4
    }
    if service_effect.effect_reply_payload_len(effect) != 4 {
        return 5
    }

    return 0
}

// Model boundary identification (Phase 160).
//
// Called after run_observability_inspection.
// State: kv count=2 (keys 5 and 20), log at capacity (4 entries).
//
// Named boundary: advisory audit silencing is invisible to the kv caller.
//
// When the log is at capacity, a kv write returns Ok but its advisory audit
// marker is silently discarded by the dispatcher.  The kv caller has no
// protocol-level signal that the audit trail is incomplete.  The effect
// model has no witness for advisory send outcomes.
//
// Pressure observed across Phase 152-159:
//   Phase 152: log restart while kv live -- audit gap during selective restart
//   Phase 156: shared namespace -- no per-client audit isolation
//   Phase 157: long-lived coherence -- state stays consistent but audit trail diverges
//   Phase 158: explicitly named advisory silencing as the one cross-service inconsistency
//   Phase 159: kv count visible, but correlation between kv writes and log
//              entries breaks once the log is full
//
// This scenario proves the boundary is real in the running artifact:
//   1. kv count and log length both at their Phase 159 post-state (2 and 4).
//   2. Two new kv writes both return Ok.
//   3. Log length is unchanged at 4: both advisory markers were dropped.
//   4. kv count is 4: both entries were stored correctly.
//   5. The gap (2 kv writes, 0 new log entries) is the named boundary.
//
// Narrowest plausible next expansion axis (named but not admitted):
//   Per-send delivery witnesses.  A witness would let the dispatcher report
//   Dropped to the kv caller when the advisory send could not be delivered.
//   This closes the gap without a recovery framework or platform.
//   No witness implementation is admitted in this phase.
func run_model_boundary(state: *boot.KernelBootState) i32 {
    effect: service_effect.Effect

    // Confirm starting state: kv has 2 entries, log is full.
    effect = boot.kernel_dispatch_step(state, serial_obs(SERIAL_ENDPOINT_ID, 1, serial_protocol.encode_kv_count()))
    if service_effect.effect_reply_payload_len(effect) != 2 {
        return 1
    }
    s: boot.KernelBootState = *state
    if log_service.log_len(s.log_state) != 4 {
        return 2
    }

    // Write two new kv entries.  Both return Ok even though the log is full.
    // The advisory markers are silently dropped; the caller sees no difference.
    effect = boot.kernel_dispatch_step(state, cmd_kv_set(30, 11))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return 3
    }
    effect = boot.kernel_dispatch_step(state, cmd_kv_set(31, 22))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return 4
    }

    // Log is still at capacity: both advisory markers were silently dropped.
    // This is the named boundary: kv success does not imply audit success.
    s = *state
    if log_service.log_len(s.log_state) != 4 {
        return 5
    }

    // kv count grew by 2: entries were stored correctly.
    effect = boot.kernel_dispatch_step(state, serial_obs(SERIAL_ENDPOINT_ID, 1, serial_protocol.encode_kv_count()))
    if service_effect.effect_reply_payload_len(effect) != 4 {
        return 6
    }

    return 0
}
