// Declarative step-table scenarios for the kernel transport path.

import boot
import kernel_dispatch
import log_service
import scenario_transport
import serial_protocol
import service_effect
import service_topology
import syscall

const STEP_COUNT: usize = 31

enum StepCheckKind {
    Routed,
    Status,
    Payload0,
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

func payload0_step(obs: syscall.ReceiveObservation, failure_code: i32, status: syscall.SyscallStatus, payload0: u8) StepSpec {
    return step(obs, failure_code, StepCheck{ kind: StepCheckKind.Payload0, status: status, reply_len: 0, payload0: payload0, payload1: 0, log_len: 0, send_dropped: 0 })
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

func step_table() [31]StepSpec {
    specs: [31]StepSpec

    specs[0] = routed_step(scenario_transport.cmd_log_append(77), 1)
    specs[1] = routed_step(scenario_transport.cmd_kv_set(5, 42), 1)
    specs[2] = payload1_step(scenario_transport.cmd_kv_get(5), 1, syscall.SyscallStatus.Ok, 42)
    specs[3] = log_tail_state_step(scenario_transport.cmd_log_tail(), 1, syscall.SyscallStatus.Ok, 77, 75)
    specs[4] = payload1_step(scenario_transport.serial_obs(service_topology.SERIAL_ENDPOINT_ID, 99, serial_protocol.encode_kv_get(5)), 1, syscall.SyscallStatus.Ok, 42)
    specs[5] = routed_step(scenario_transport.cmd_kv_set(5, 77), 1)
    specs[6] = routed_step(scenario_transport.cmd_kv_set(5, 99), 1)
    specs[7] = payload1_step(scenario_transport.cmd_kv_get(5), 1, syscall.SyscallStatus.Ok, 99)
    specs[8] = payload1_step(scenario_transport.cmd_kv_get(5), 1, syscall.SyscallStatus.Ok, 99)
    specs[9] = status_step(scenario_transport.cmd_log_append(42), 1, syscall.SyscallStatus.Exhausted)
    specs[10] = status_step(scenario_transport.cmd_kv_set(20, 55), 1, syscall.SyscallStatus.Ok)
    specs[11] = status_step(scenario_transport.cmd_log_append(1), 1, syscall.SyscallStatus.Exhausted)
    specs[12] = status_step(scenario_transport.cmd_kv_get(99), 1, syscall.SyscallStatus.InvalidArgument)
    specs[13] = status_step(scenario_transport.cmd_kv_set(20, 77), 1, syscall.SyscallStatus.Ok)
    specs[14] = reply_len_step(scenario_transport.kv_count_obs(1), 1, syscall.SyscallStatus.Ok, 2)
    specs[15] = reply_len_step(scenario_transport.kv_count_obs(1), 1, syscall.SyscallStatus.Ok, 2)
    specs[16] = reply_len_step(scenario_transport.cmd_log_tail(), 1, syscall.SyscallStatus.Ok, 4)
    specs[17] = reply_len_log_len_step(scenario_transport.kv_count_obs(1), 6, syscall.SyscallStatus.Ok, 2, 4)
    specs[18] = status_step(scenario_transport.cmd_kv_set(30, 11), 6, syscall.SyscallStatus.Ok)
    specs[19] = reply_len_log_len_step(scenario_transport.cmd_kv_set(31, 22), 6, syscall.SyscallStatus.Ok, 0, 4)
    specs[20] = reply_len_step(scenario_transport.kv_count_obs(1), 6, syscall.SyscallStatus.Ok, 4)
    specs[21] = witness_step(scenario_transport.cmd_kv_set(5, 123), 7, syscall.SyscallStatus.Ok, 1)
    specs[22] = payload1_step(scenario_transport.cmd_echo(7, 8), 10, syscall.SyscallStatus.Ok, 8)
    specs[23] = reply_len_step(scenario_transport.cmd_echo(1, 2), 10, syscall.SyscallStatus.Ok, 2)
    specs[24] = reply_len_step(scenario_transport.cmd_echo(3, 4), 10, syscall.SyscallStatus.Ok, 2)
    specs[25] = reply_len_step(scenario_transport.cmd_echo(5, 6), 10, syscall.SyscallStatus.Ok, 2)
    specs[26] = status_step(scenario_transport.cmd_echo(9, 10), 11, syscall.SyscallStatus.Exhausted)
    specs[27] = status_step(scenario_transport.cmd_queue_enqueue(41), 40, syscall.SyscallStatus.Ok)
    specs[28] = status_step(scenario_transport.cmd_queue_enqueue(42), 40, syscall.SyscallStatus.Ok)
    specs[29] = payload0_step(scenario_transport.cmd_queue_dequeue(), 40, syscall.SyscallStatus.Ok, 41)
    specs[30] = payload0_step(scenario_transport.cmd_queue_dequeue(), 40, syscall.SyscallStatus.Ok, 42)

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
    switch check.kind {
    case StepCheckKind.Status:
        return 1
    case StepCheckKind.Payload0:
        if service_effect.effect_reply_payload(effect)[0] != check.payload0 {
            return 0
        }
        return 1
    case StepCheckKind.Payload1:
        if service_effect.effect_reply_payload(effect)[1] != check.payload1 {
            return 0
        }
        return 1
    case StepCheckKind.LogTailState:
        payload: [4]u8 = service_effect.effect_reply_payload(effect)
        if service_effect.effect_reply_payload_len(effect) != log_service.log_len(s.log.state) {
            return 0
        }
        if payload[0] != check.payload0 {
            return 0
        }
        if payload[1] != check.payload1 {
            return 0
        }
        return 1
    case StepCheckKind.ReplyLen:
        if service_effect.effect_reply_payload_len(effect) != check.reply_len {
            return 0
        }
        return 1
    case StepCheckKind.ReplyLenAndLogLen:
        if service_effect.effect_reply_payload_len(effect) != check.reply_len {
            return 0
        }
        if log_service.log_len(s.log.state) != check.log_len {
            return 0
        }
        return 1
    case StepCheckKind.Witness:
        if service_effect.effect_send_dropped(effect) != check.send_dropped {
            return 0
        }
        return 1
    default:
        return 0
    }
}

func run_transport_step_table_probe(state: *boot.KernelBootState) i32 {
    specs: [31]StepSpec = step_table()

    for step in 0..STEP_COUNT {
        spec: StepSpec = specs[step]
        effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(state, spec.obs)
        if step_matches(spec, state, effect) == 0 {
            return spec.failure_code
        }
    }
    return 0
}