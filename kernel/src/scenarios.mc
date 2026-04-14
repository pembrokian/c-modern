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
