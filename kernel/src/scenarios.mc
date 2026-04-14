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
