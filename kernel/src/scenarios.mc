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
import ipc
import init
import kernel_dispatch
import log_service
import serial_protocol
import service_effect
import service_identity
import service_topology
import shell_service
import syscall
import ticket_service

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

func cmd_echo(left: u8, right: u8) syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_echo(left, right))
}

func cmd_kv_set(key: u8, value: u8) syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_kv_set(key, value))
}

func cmd_kv_get(key: u8) syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_kv_get(key))
}

func cmd_lifecycle_query(target: u8) syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_lifecycle_query(target))
}

func cmd_lifecycle_restart(target: u8) syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_lifecycle_restart(target))
}

func cmd_queue_enqueue(value: u8) syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_queue_enqueue(value))
}

func cmd_queue_dequeue() syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_queue_dequeue())
}

func cmd_ticket_issue() syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_ticket_issue())
}

func cmd_ticket_use(epoch: u8, id: u8) syscall.ReceiveObservation {
    return serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, serial_protocol.encode_ticket_use(epoch, id))
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
    specs[22] = payload1_step(cmd_echo(7, 8), 10, syscall.SyscallStatus.Ok, 8)
    specs[23] = reply_len_step(cmd_echo(1, 2), 10, syscall.SyscallStatus.Ok, 2)
    specs[24] = reply_len_step(cmd_echo(3, 4), 10, syscall.SyscallStatus.Ok, 2)
    specs[25] = reply_len_step(cmd_echo(5, 6), 10, syscall.SyscallStatus.Ok, 2)
    specs[26] = status_step(cmd_echo(9, 10), 11, syscall.SyscallStatus.Exhausted)
    specs[27] = status_step(cmd_queue_enqueue(41), 40, syscall.SyscallStatus.Ok)
    specs[28] = status_step(cmd_queue_enqueue(42), 40, syscall.SyscallStatus.Ok)
    specs[29] = payload0_step(cmd_queue_dequeue(), 40, syscall.SyscallStatus.Ok, 41)
    specs[30] = payload0_step(cmd_queue_dequeue(), 40, syscall.SyscallStatus.Ok, 42)

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
    if check.kind == StepCheckKind.Payload0 {
        if service_effect.effect_reply_payload(effect)[0] != check.payload0 {
            return 0
        }
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
        if log_service.log_len(s.log.state) != check.log_len {
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

func expect_restart_identity(before: service_identity.ServiceMark, after: service_identity.ServiceMark, base: i32) i32 {
    if !service_identity.marks_same_endpoint(before, after) {
        return base
    }
    if !service_identity.marks_same_pid(before, after) {
        return base + 1
    }
    if service_identity.marks_same_instance(before, after) {
        return base + 2
    }
    if service_identity.mark_generation(after) != service_identity.mark_generation(before) + 1 {
        return base + 3
    }
    return 0
}

func expect_lifecycle(effect: service_effect.Effect, status: syscall.SyscallStatus, target: u8, mode: u8) bool {
    if service_effect.effect_reply_status(effect) != status {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 2 {
        return false
    }
    payload: [4]u8 = service_effect.effect_reply_payload(effect)
    if payload[0] != target {
        return false
    }
    if payload[1] != mode {
        return false
    }
    return true
}

// run_restart_probe verifies two restart contracts on the live path:
// log restart reloads the retained state snapshot exactly as saved, and echo
// restart still resets its bounded request counter.
func run_restart_probe(state: *boot.KernelBootState) i32 {
    log_before: service_identity.ServiceMark = boot.boot_log_mark(*state)
    *state = init.restart(*state, service_topology.LOG_ENDPOINT_ID)
    log_after: service_identity.ServiceMark = boot.boot_log_mark(*state)

    log_id: i32 = expect_restart_identity(log_before, log_after, 8)
    if log_id != 0 {
        return log_id
    }

    // The reloaded log stays full after restart because the retained entries
    // were explicitly reloaded rather than discarded.
    tail_effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(state, cmd_log_tail())
    if service_effect.effect_reply_status(tail_effect) != syscall.SyscallStatus.Ok {
        return 12
    }
    if service_effect.effect_reply_payload_len(tail_effect) != 4 {
        return 13
    }
    tail_payload: [4]u8 = service_effect.effect_reply_payload(tail_effect)
    if tail_payload[0] != 77 {
        return 14
    }
    if tail_payload[1] != 75 {
        return 15
    }
    if tail_payload[2] != 75 {
        return 16
    }
    if tail_payload[3] != 75 {
        return 17
    }

    append_effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(state, cmd_log_append(55))
    if service_effect.effect_reply_status(append_effect) != syscall.SyscallStatus.Exhausted {
        return 18
    }

    kv_before: service_identity.ServiceMark = boot.boot_kv_mark(*state)
    *state = init.restart(*state, service_topology.KV_ENDPOINT_ID)
    kv_after: service_identity.ServiceMark = boot.boot_kv_mark(*state)

    kv_id: i32 = expect_restart_identity(kv_before, kv_after, 19)
    if kv_id != 0 {
        return kv_id
    }

    kv_count_effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(state, kv_count_obs(1))
    if service_effect.effect_reply_status(kv_count_effect) != syscall.SyscallStatus.Ok {
        return 23
    }
    if service_effect.effect_reply_payload_len(kv_count_effect) != 4 {
        return 24
    }

    kv_effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(state, cmd_kv_get(20))
    if service_effect.effect_reply_status(kv_effect) != syscall.SyscallStatus.Ok {
        return 25
    }
    if service_effect.effect_reply_payload_len(kv_effect) != 2 {
        return 26
    }
    kv_payload: [4]u8 = service_effect.effect_reply_payload(kv_effect)
    if kv_payload[0] != 20 {
        return 27
    }
    if kv_payload[1] != 77 {
        return 28
    }

    kv_overwrite_effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(state, cmd_kv_set(20, 88))
    if service_effect.effect_reply_status(kv_overwrite_effect) != syscall.SyscallStatus.Ok {
        return 29
    }

    kv_effect = kernel_dispatch.kernel_dispatch_step(state, cmd_kv_get(20))
    if service_effect.effect_reply_status(kv_effect) != syscall.SyscallStatus.Ok {
        return 30
    }
    if service_effect.effect_reply_payload(kv_effect)[1] != 88 {
        return 31
    }

    queue_before: service_identity.ServiceMark = boot.boot_queue_mark(*state)
    queue_effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(state, cmd_queue_enqueue(61))
    if service_effect.effect_reply_status(queue_effect) != syscall.SyscallStatus.Ok {
        return 40
    }
    queue_effect = kernel_dispatch.kernel_dispatch_step(state, cmd_queue_enqueue(62))
    if service_effect.effect_reply_status(queue_effect) != syscall.SyscallStatus.Ok {
        return 41
    }
    queue_effect = kernel_dispatch.kernel_dispatch_step(state, cmd_queue_enqueue(63))
    if service_effect.effect_reply_status(queue_effect) != syscall.SyscallStatus.Ok {
        return 42
    }

    *state = init.restart(*state, service_topology.QUEUE_ENDPOINT_ID)
    queue_after: service_identity.ServiceMark = boot.boot_queue_mark(*state)

    queue_id: i32 = expect_restart_identity(queue_before, queue_after, 43)
    if queue_id != 0 {
        return queue_id
    }

    queue_effect = kernel_dispatch.kernel_dispatch_step(state, cmd_queue_dequeue())
    if service_effect.effect_reply_status(queue_effect) != syscall.SyscallStatus.Ok {
        return 47
    }
    if service_effect.effect_reply_payload(queue_effect)[0] != 61 {
        return 48
    }

    queue_effect = kernel_dispatch.kernel_dispatch_step(state, cmd_queue_dequeue())
    if service_effect.effect_reply_status(queue_effect) != syscall.SyscallStatus.Ok {
        return 49
    }
    if service_effect.effect_reply_payload(queue_effect)[0] != 62 {
        return 50
    }

    queue_effect = kernel_dispatch.kernel_dispatch_step(state, cmd_queue_enqueue(64))
    if service_effect.effect_reply_status(queue_effect) != syscall.SyscallStatus.Ok {
        return 51
    }

    queue_effect = kernel_dispatch.kernel_dispatch_step(state, cmd_queue_dequeue())
    if service_effect.effect_reply_status(queue_effect) != syscall.SyscallStatus.Ok {
        return 52
    }
    if service_effect.effect_reply_payload(queue_effect)[0] != 63 {
        return 53
    }

    queue_effect = kernel_dispatch.kernel_dispatch_step(state, cmd_queue_dequeue())
    if service_effect.effect_reply_status(queue_effect) != syscall.SyscallStatus.Ok {
        return 54
    }
    if service_effect.effect_reply_payload(queue_effect)[0] != 64 {
        return 55
    }

    queue_effect = kernel_dispatch.kernel_dispatch_step(state, cmd_queue_dequeue())
    if service_effect.effect_reply_status(queue_effect) != syscall.SyscallStatus.InvalidArgument {
        return 56
    }

    echo_before: service_identity.ServiceMark = boot.boot_echo_mark(*state)
    *state = init.restart(*state, service_topology.ECHO_ENDPOINT_ID)
    echo_after: service_identity.ServiceMark = boot.boot_echo_mark(*state)

    echo_id: i32 = expect_restart_identity(echo_before, echo_after, 32)
    if echo_id != 0 {
        return echo_id
    }

    echo_effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(state, cmd_echo(33, 44))
    if service_effect.effect_reply_status(echo_effect) != syscall.SyscallStatus.Ok {
        return 57
    }
    if service_effect.effect_reply_payload_len(echo_effect) != 2 {
        return 58
    }
    if service_effect.effect_reply_payload(echo_effect)[0] != 33 {
        return 59
    }
    if service_effect.effect_reply_payload(echo_effect)[1] != 44 {
        return 60
    }

    ticket_before: service_identity.ServiceMark = boot.boot_ticket_mark(*state)
    ticket_issue_effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(state, cmd_ticket_issue())
    if service_effect.effect_reply_status(ticket_issue_effect) != syscall.SyscallStatus.Ok {
        return 61
    }
    if service_effect.effect_reply_payload_len(ticket_issue_effect) != 2 {
        return 62
    }
    stale_ticket: [4]u8 = service_effect.effect_reply_payload(ticket_issue_effect)

    *state = init.restart(*state, service_topology.TICKET_ENDPOINT_ID)
    ticket_after: service_identity.ServiceMark = boot.boot_ticket_mark(*state)

    ticket_id: i32 = expect_restart_identity(ticket_before, ticket_after, 63)
    if ticket_id != 0 {
        return ticket_id
    }

    ticket_use_effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(state, cmd_ticket_use(stale_ticket[0], stale_ticket[1]))
    if service_effect.effect_reply_status(ticket_use_effect) != syscall.SyscallStatus.InvalidArgument {
        return 67
    }
    if service_effect.effect_reply_payload_len(ticket_use_effect) != 1 {
        return 68
    }
    if service_effect.effect_reply_payload(ticket_use_effect)[0] != ticket_service.TICKET_STALE {
        return 69
    }

    ticket_issue_effect = kernel_dispatch.kernel_dispatch_step(state, cmd_ticket_issue())
    if service_effect.effect_reply_status(ticket_issue_effect) != syscall.SyscallStatus.Ok {
        return 70
    }
    if service_effect.effect_reply_payload_len(ticket_issue_effect) != 2 {
        return 71
    }
    fresh_ticket: [4]u8 = service_effect.effect_reply_payload(ticket_issue_effect)
    if fresh_ticket[0] == stale_ticket[0] {
        return 72
    }

    ticket_use_effect = kernel_dispatch.kernel_dispatch_step(state, cmd_ticket_use(fresh_ticket[0], fresh_ticket[1]))
    if service_effect.effect_reply_status(ticket_use_effect) != syscall.SyscallStatus.Ok {
        return 73
    }
    if service_effect.effect_reply_payload_len(ticket_use_effect) != 1 {
        return 74
    }
    if service_effect.effect_reply_payload(ticket_use_effect)[0] != fresh_ticket[1] {
        return 75
    }

    ticket_use_effect = kernel_dispatch.kernel_dispatch_step(state, cmd_ticket_use(fresh_ticket[0], fresh_ticket[1]))
    if service_effect.effect_reply_status(ticket_use_effect) != syscall.SyscallStatus.InvalidArgument {
        return 76
    }
    if service_effect.effect_reply_payload_len(ticket_use_effect) != 1 {
        return 77
    }
    if service_effect.effect_reply_payload(ticket_use_effect)[0] != ticket_service.TICKET_INVALID {
        return 78
    }

    return 0
}

func run_shell_lifecycle_probe(state: *boot.KernelBootState) i32 {
    effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(state, cmd_lifecycle_query(serial_protocol.TARGET_QUEUE))
    if !expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_QUEUE, serial_protocol.LIFECYCLE_RELOAD) {
        return 79
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, cmd_lifecycle_query(serial_protocol.TARGET_ECHO))
    if !expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_ECHO, serial_protocol.LIFECYCLE_RESET) {
        return 80
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, cmd_lifecycle_query(serial_protocol.TARGET_SERIAL))
    if !expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_SERIAL, serial_protocol.LIFECYCLE_NONE) {
        return 81
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, cmd_lifecycle_restart(serial_protocol.TARGET_SERIAL))
    if !expect_lifecycle(effect, syscall.SyscallStatus.InvalidArgument, serial_protocol.TARGET_SERIAL, serial_protocol.LIFECYCLE_NONE) {
        return 82
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, cmd_queue_enqueue(71))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return 83
    }
    effect = kernel_dispatch.kernel_dispatch_step(state, cmd_queue_enqueue(72))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return 84
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, cmd_lifecycle_restart(serial_protocol.TARGET_QUEUE))
    if !expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_QUEUE, serial_protocol.LIFECYCLE_RELOAD) {
        return 85
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, cmd_queue_dequeue())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return 86
    }
    if service_effect.effect_reply_payload(effect)[0] != 71 {
        return 87
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, cmd_queue_dequeue())
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return 88
    }
    if service_effect.effect_reply_payload(effect)[0] != 72 {
        return 89
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, cmd_lifecycle_restart(serial_protocol.TARGET_ECHO))
    if !expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_ECHO, serial_protocol.LIFECYCLE_RESET) {
        return 90
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, cmd_echo(41, 42))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return 91
    }
    effect = kernel_dispatch.kernel_dispatch_step(state, cmd_echo(43, 44))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return 92
    }
    effect = kernel_dispatch.kernel_dispatch_step(state, cmd_echo(45, 46))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return 93
    }
    effect = kernel_dispatch.kernel_dispatch_step(state, cmd_echo(47, 48))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return 94
    }
    effect = kernel_dispatch.kernel_dispatch_step(state, cmd_echo(49, 50))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Exhausted {
        return 95
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, cmd_lifecycle_restart(serial_protocol.TARGET_ECHO))
    if !expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_ECHO, serial_protocol.LIFECYCLE_RESET) {
        return 96
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, cmd_echo(51, 52))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return 97
    }
    if service_effect.effect_reply_payload(effect)[0] != 51 {
        return 98
    }
    if service_effect.effect_reply_payload(effect)[1] != 52 {
        return 99
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, cmd_lifecycle_query(serial_protocol.TARGET_LOG))
    if !expect_lifecycle(effect, syscall.SyscallStatus.Ok, serial_protocol.TARGET_LOG, serial_protocol.LIFECYCLE_RELOAD) {
        return 100
    }

    effect = kernel_dispatch.kernel_dispatch_step(state, serial_obs(DEFAULT_SERIAL_ROUTE.endpoint, DEFAULT_SERIAL_ROUTE.pid, ipc.payload_byte(serial_protocol.CMD_X, serial_protocol.CMD_G, serial_protocol.TARGET_LOG, serial_protocol.CMD_BANG)))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.InvalidArgument {
        return 101
    }
    if service_effect.effect_reply_payload_len(effect) != 2 {
        return 102
    }
    if service_effect.effect_reply_payload(effect)[0] != shell_service.SHELL_INVALID_REPLY {
        return 103
    }
    if service_effect.effect_reply_payload(effect)[1] != shell_service.SHELL_INVALID_COMMAND {
        return 104
    }

    return 0
}

func run(state: *boot.KernelBootState) i32 {
    result: i32 = run_main(state)
    if result != 0 {
        return result
    }
    result = run_restart_probe(state)
    if result != 0 {
        return result
    }
    return run_shell_lifecycle_probe(state)
}
