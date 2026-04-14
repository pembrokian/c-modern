// Integration loop over the boot kernel state.
//
// run(state) is a small scripted event loop: it receives one observation,
// dispatches it through kernel_dispatch_step, then delivers the returned
// effect to the current integration checks before moving to the next event.
// All events share one boot state, so later steps observe real retained
// state written by earlier ones.
//
// This module also owns the transport adapter layer: serial_obs wraps a
// protocol payload into a syscall.ReceiveObservation.  endpoint and pid are
// explicit parameters so the transport can be swapped without touching the
// protocol encoding in serial_protocol.mc.

import boot
import init
import kernel_dispatch
import log_service
import serial_protocol
import service_effect
import service_topology
import syscall

const STEP_COUNT: usize = 22

enum StepCheckKind {
    Routed,
    Status,
    Payload1,
    LogTailState,
    ReplyLen,
    ReplyLenAndLogLen,
    Witness,
}

struct StepCheck {
    kind: StepCheckKind
    status: syscall.SyscallStatus
    reply_len: usize
    payload0: u8
    payload1: u8
    log_len: usize
    send_dropped: u32
}

struct StepSpec {
    obs: syscall.ReceiveObservation
    failure_code: i32
    check: StepCheck
}

struct SerialRoute {
    endpoint: u32
    pid: u32
}

const DEFAULT_SERIAL_ROUTE: SerialRoute = SerialRoute{ endpoint: service_topology.SERIAL_ENDPOINT_ID, pid: 1 }

// serial_obs wraps a protocol payload into a ReceiveObservation.
// endpoint and pid are explicit: swap the endpoint to route to a different
// service; swap pid to simulate a different client.
func serial_obs(endpoint: u32, pid: u32, payload: [4]u8) syscall.ReceiveObservation {
    return syscall.ReceiveObservation{ status: syscall.SyscallStatus.Ok, block_reason: syscall.BlockReason.None, endpoint_id: endpoint, source_pid: pid, payload_len: 4, received_handle_slot: 0, received_handle_count: 0, payload: payload }
}

// cmd_* are single-client convenience wrappers: serial endpoint, pid=1.
func cmd_log_append(value: u8) syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_log_append(value))
}

func cmd_log_tail() syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_log_tail())
}

func cmd_kv_set(key: u8, value: u8) syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_kv_set(key, value))
}

func cmd_kv_get(key: u8) syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_kv_get(key))
}

func kv_count_obs(pid: u32) syscall.ReceiveObservation {
    return serial_obs(service_topology.SERIAL_ENDPOINT_ID, pid, serial_protocol.encode_kv_count())
}

func step(obs: syscall.ReceiveObservation, failure_code: i32, check: StepCheck) StepSpec {
    return StepSpec{ obs: obs, failure_code: failure_code, check: check }
}

func routed_step(obs: syscall.ReceiveObservation, failure_code: i32) StepSpec {
    return step(obs, failure_code, StepCheck{ kind: StepCheckKind.Routed, status: syscall.SyscallStatus.None, reply_len: 0, payload0: 0, payload1: 0, log_len: 0, send_dropped: 0 })
}

func status_step(obs: syscall.ReceiveObservation, failure_code: i32, status: syscall.SyscallStatus) StepSpec {
    return step(obs, failure_code, StepCheck{ kind: StepCheckKind.Status, status: status, reply_len: 0, payload0: 0, payload1: 0, log_len: 0, send_dropped: 0 })
}

func payload1_step(obs: syscall.ReceiveObservation, failure_code: i32, status: syscall.SyscallStatus, payload1: u8) StepSpec {
    return step(obs, failure_code, StepCheck{ kind: StepCheckKind.Payload1, status: status, reply_len: 0, payload0: 0, payload1: payload1, log_len: 0, send_dropped: 0 })
}

func log_tail_state_step(obs: syscall.ReceiveObservation, failure_code: i32, status: syscall.SyscallStatus, payload0: u8, payload1: u8) StepSpec {
    return step(obs, failure_code, StepCheck{ kind: StepCheckKind.LogTailState, status: status, reply_len: 0, payload0: payload0, payload1: payload1, log_len: 0, send_dropped: 0 })
}

func reply_len_step(obs: syscall.ReceiveObservation, failure_code: i32, status: syscall.SyscallStatus, reply_len: usize) StepSpec {
    return step(obs, failure_code, StepCheck{ kind: StepCheckKind.ReplyLen, status: status, reply_len: reply_len, payload0: 0, payload1: 0, log_len: 0, send_dropped: 0 })
}

func reply_len_log_len_step(obs: syscall.ReceiveObservation, failure_code: i32, status: syscall.SyscallStatus, reply_len: usize, log_len: usize) StepSpec {
    return step(obs, failure_code, StepCheck{ kind: StepCheckKind.ReplyLenAndLogLen, status: status, reply_len: reply_len, payload0: 0, payload1: 0, log_len: log_len, send_dropped: 0 })
}

func witness_step(obs: syscall.ReceiveObservation, failure_code: i32, status: syscall.SyscallStatus, send_dropped: u32) StepSpec {
    return step(obs, failure_code, StepCheck{ kind: StepCheckKind.Witness, status: status, reply_len: 0, payload0: 0, payload1: 0, log_len: 0, send_dropped: send_dropped })
}

func step_table() [22]StepSpec {
    specs: [22]StepSpec

    specs[0] = routed_step(cmd_log_append(77), 1)
    specs[1] = routed_step(cmd_kv_set(5, 42), 1)
    specs[2] = payload1_step(cmd_kv_get(5), 1, syscall.SyscallStatus.Ok, 42)
    specs[3] = log_tail_state_step(cmd_log_tail(), 1, syscall.SyscallStatus.Ok, 77, 75)
    specs[4] = payload1_step(serial_obs(service_topology.SERIAL_ENDPOINT_ID, 99, serial_protocol.encode_kv_get(5)), 1, syscall.SyscallStatus.Ok, 42)
    specs[5] = routed_step(cmd_kv_set(5, 77), 1)
    specs[6] = routed_step(cmd_kv_set(5, 99), 1)
    specs[7] = payload1_step(cmd_kv_get(5), 1, syscall.SyscallStatus.Ok, 99)
    specs[8] = payload1_step(cmd_kv_get(5), 1, syscall.SyscallStatus.Ok, 99)
    specs[9] = status_step(cmd_log_append(42), 1, syscall.SyscallStatus.Exhausted)
    specs[10] = status_step(cmd_kv_set(20, 55), 1, syscall.SyscallStatus.Ok)
    specs[11] = status_step(cmd_log_append(1), 1, syscall.SyscallStatus.Exhausted)
    specs[12] = status_step(cmd_kv_get(99), 1, syscall.SyscallStatus.InvalidArgument)
    specs[13] = status_step(cmd_kv_set(20, 77), 1, syscall.SyscallStatus.Ok)
    specs[14] = reply_len_step(kv_count_obs(1), 1, syscall.SyscallStatus.Ok, 2)
    specs[15] = reply_len_step(kv_count_obs(1), 1, syscall.SyscallStatus.Ok, 2)
    specs[16] = reply_len_step(cmd_log_tail(), 1, syscall.SyscallStatus.Ok, 4)
    specs[17] = reply_len_log_len_step(kv_count_obs(1), 6, syscall.SyscallStatus.Ok, 2, 4)
    specs[18] = status_step(cmd_kv_set(30, 11), 6, syscall.SyscallStatus.Ok)
    specs[19] = reply_len_log_len_step(cmd_kv_set(31, 22), 6, syscall.SyscallStatus.Ok, 0, 4)
    specs[20] = reply_len_step(kv_count_obs(1), 6, syscall.SyscallStatus.Ok, 4)
    specs[21] = witness_step(cmd_kv_set(5, 123), 7, syscall.SyscallStatus.Ok, 1)

    return specs
}

func step_matches(spec: StepSpec, state: *boot.KernelBootState, effect: service_effect.Effect) u32 {
    s: boot.KernelBootState = *state
    check: StepCheck = spec.check

    if check.kind == StepCheckKind.Routed {
        return boot.debug_boot_routed(effect)
    }
    if service_effect.effect_reply_status(effect) != check.status {
        return 0
    }
    if check.kind == StepCheckKind.Status {
        return 1
    }
    if check.kind == StepCheckKind.Payload1 {
        if service_effect.effect_reply_payload(effect)[1] != check.payload1 {
            return 0
        }
        return 1
    }
    if check.kind == StepCheckKind.LogTailState {
        payload: [4]u8 = service_effect.effect_reply_payload(effect)
        if service_effect.effect_reply_payload_len(effect) != log_service.log_len(s.log_state) {
            return 0
        }
        if payload[0] != check.payload0 {
            return 0
        }
        if payload[1] != check.payload1 {
            return 0
        }
        return 1
    }
    if check.kind == StepCheckKind.ReplyLen {
        if service_effect.effect_reply_payload_len(effect) != check.reply_len {
            return 0
        }
        return 1
    }
    if check.kind == StepCheckKind.ReplyLenAndLogLen {
        if service_effect.effect_reply_payload_len(effect) != check.reply_len {
            return 0
        }
        if log_service.log_len(s.log_state) != check.log_len {
            return 0
        }
        return 1
    }
    if check.kind == StepCheckKind.Witness {
        if service_effect.effect_send_dropped(effect) != check.send_dropped {
            return 0
        }
        return 1
    }
    return 0
}

func run_main(state: *boot.KernelBootState) i32 {
    specs: [22]StepSpec = step_table()

    for step in 0..STEP_COUNT {
        spec: StepSpec = specs[step]
        effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(state, spec.obs)
        if step_matches(spec, state, effect) == 0 {
            return spec.failure_code
        }
    }
    return 0
}

// run_restart_probe verifies service restart: the log is exhausted after
// run_main, init.restart_log replaces its state, and the log must then
// accept a new append and report one retained entry.
func run_restart_probe(state: *boot.KernelBootState) i32 {
    *state = init.restart_log(*state)

    // After restart the log must accept a new append.
    append_effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(state, cmd_log_append(55))
    if service_effect.effect_reply_status(append_effect) != syscall.SyscallStatus.Ok {
        return 8
    }

    // Tail must report exactly one retained entry.
    tail_effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(state, cmd_log_tail())
    if service_effect.effect_reply_payload_len(tail_effect) != 1 {
        return 9
    }

    return 0
}

func run(state: *boot.KernelBootState) i32 {
    result: i32 = run_main(state)
    if result != 0 {
        return result
    }
    return run_restart_probe(state)
}
