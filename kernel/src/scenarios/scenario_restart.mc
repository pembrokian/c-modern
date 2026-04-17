// Restart contract probes for retained and reset-style services.

import boot
import init
import kernel_dispatch
import scenario_assert
import scenario_transport
import service_effect
import service_identity
import service_topology
import ticket_service
import syscall

const FAIL_RESTART_LOG_IDENTITY_BASE: i32 = 8
const FAIL_RESTART_LOG_TAIL_STATUS: i32 = 12
const FAIL_RESTART_LOG_TAIL_LEN: i32 = 13
const FAIL_RESTART_LOG_TAIL_0: i32 = 14
const FAIL_RESTART_LOG_TAIL_1: i32 = 15
const FAIL_RESTART_LOG_TAIL_2: i32 = 16
const FAIL_RESTART_LOG_TAIL_3: i32 = 17
const FAIL_RESTART_LOG_APPEND_STATUS: i32 = 18
const FAIL_RESTART_KV_IDENTITY_BASE: i32 = 19
const FAIL_RESTART_KV_COUNT_STATUS: i32 = 23
const FAIL_RESTART_KV_COUNT_LEN: i32 = 24
const FAIL_RESTART_KV_GET_STATUS: i32 = 25
const FAIL_RESTART_KV_GET_LEN: i32 = 26
const FAIL_RESTART_KV_GET_KEY: i32 = 27
const FAIL_RESTART_KV_GET_VALUE: i32 = 28
const FAIL_RESTART_KV_OVERWRITE_STATUS: i32 = 29
const FAIL_RESTART_KV_SECOND_GET_STATUS: i32 = 30
const FAIL_RESTART_KV_SECOND_GET_VALUE: i32 = 31
const FAIL_RESTART_QUEUE_ENQUEUE_0: i32 = 40
const FAIL_RESTART_QUEUE_ENQUEUE_1: i32 = 41
const FAIL_RESTART_QUEUE_ENQUEUE_2: i32 = 42
const FAIL_RESTART_QUEUE_IDENTITY_BASE: i32 = 43
const FAIL_RESTART_QUEUE_DEQUEUE_0_STATUS: i32 = 47
const FAIL_RESTART_QUEUE_DEQUEUE_0_VALUE: i32 = 48
const FAIL_RESTART_QUEUE_DEQUEUE_1_STATUS: i32 = 49
const FAIL_RESTART_QUEUE_DEQUEUE_1_VALUE: i32 = 50
const FAIL_RESTART_QUEUE_ENQUEUE_3_STATUS: i32 = 51
const FAIL_RESTART_QUEUE_DEQUEUE_2_STATUS: i32 = 52
const FAIL_RESTART_QUEUE_DEQUEUE_2_VALUE: i32 = 53
const FAIL_RESTART_QUEUE_DEQUEUE_3_STATUS: i32 = 54
const FAIL_RESTART_QUEUE_DEQUEUE_3_VALUE: i32 = 55
const FAIL_RESTART_QUEUE_EMPTY_STATUS: i32 = 56
const FAIL_RESTART_ECHO_IDENTITY_BASE: i32 = 32
const FAIL_RESTART_ECHO_STATUS: i32 = 57
const FAIL_RESTART_ECHO_LEN: i32 = 58
const FAIL_RESTART_ECHO_PAYLOAD_0: i32 = 59
const FAIL_RESTART_ECHO_PAYLOAD_1: i32 = 60
const FAIL_RESTART_TICKET_ISSUE_STATUS: i32 = 61
const FAIL_RESTART_TICKET_ISSUE_LEN: i32 = 62
const FAIL_RESTART_TICKET_IDENTITY_BASE: i32 = 63
const FAIL_RESTART_TICKET_STALE_STATUS: i32 = 67
const FAIL_RESTART_TICKET_STALE_LEN: i32 = 68
const FAIL_RESTART_TICKET_STALE_KIND: i32 = 69
const FAIL_RESTART_TICKET_FRESH_ISSUE_STATUS: i32 = 70
const FAIL_RESTART_TICKET_FRESH_ISSUE_LEN: i32 = 71
const FAIL_RESTART_TICKET_EPOCH_REUSED: i32 = 72
const FAIL_RESTART_TICKET_USE_STATUS: i32 = 73
const FAIL_RESTART_TICKET_USE_LEN: i32 = 74
const FAIL_RESTART_TICKET_USE_VALUE: i32 = 75
const FAIL_RESTART_TICKET_INVALID_STATUS: i32 = 76
const FAIL_RESTART_TICKET_INVALID_LEN: i32 = 77
const FAIL_RESTART_TICKET_INVALID_KIND: i32 = 78

func run_restart_probe(state: *boot.KernelBootState) i32 {
    log_before: service_identity.ServiceMark = boot.boot_log_mark(*state)
    *state = init.restart(*state, service_topology.LOG_ENDPOINT_ID)
    log_after: service_identity.ServiceMark = boot.boot_log_mark(*state)

    log_before_endpoint: u32 = service_identity.mark_endpoint(log_before)
    log_before_pid: u32 = service_identity.mark_pid(log_before)
    log_before_generation: u32 = service_identity.mark_generation(log_before)
    log_after_endpoint: u32 = service_identity.mark_endpoint(log_after)
    log_after_pid: u32 = service_identity.mark_pid(log_after)
    log_after_generation: u32 = service_identity.mark_generation(log_after)
    log_id: i32 = scenario_assert.expect_restart_identity(log_before_endpoint, log_before_pid, log_before_generation, log_after_endpoint, log_after_pid, log_after_generation, FAIL_RESTART_LOG_IDENTITY_BASE)
    if log_id != 0 {
        return log_id
    }

    tail_effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_log_tail())
    if service_effect.effect_reply_status(tail_effect) != syscall.SyscallStatus.Ok {
        return FAIL_RESTART_LOG_TAIL_STATUS
    }
    if service_effect.effect_reply_payload_len(tail_effect) != 4 {
        return FAIL_RESTART_LOG_TAIL_LEN
    }
    tail_payload: [4]u8 = service_effect.effect_reply_payload(tail_effect)
    if tail_payload[0] != 77 {
        return FAIL_RESTART_LOG_TAIL_0
    }
    if tail_payload[1] != 75 {
        return FAIL_RESTART_LOG_TAIL_1
    }
    if tail_payload[2] != 75 {
        return FAIL_RESTART_LOG_TAIL_2
    }
    if tail_payload[3] != 75 {
        return FAIL_RESTART_LOG_TAIL_3
    }

    append_effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_log_append(55))
    if service_effect.effect_reply_status(append_effect) != syscall.SyscallStatus.Exhausted {
        return FAIL_RESTART_LOG_APPEND_STATUS
    }

    kv_before: service_identity.ServiceMark = boot.boot_kv_mark(*state)
    *state = init.restart(*state, service_topology.KV_ENDPOINT_ID)
    kv_after: service_identity.ServiceMark = boot.boot_kv_mark(*state)

    kv_before_endpoint: u32 = service_identity.mark_endpoint(kv_before)
    kv_before_pid: u32 = service_identity.mark_pid(kv_before)
    kv_before_generation: u32 = service_identity.mark_generation(kv_before)
    kv_after_endpoint: u32 = service_identity.mark_endpoint(kv_after)
    kv_after_pid: u32 = service_identity.mark_pid(kv_after)
    kv_after_generation: u32 = service_identity.mark_generation(kv_after)
    kv_id: i32 = scenario_assert.expect_restart_identity(kv_before_endpoint, kv_before_pid, kv_before_generation, kv_after_endpoint, kv_after_pid, kv_after_generation, FAIL_RESTART_KV_IDENTITY_BASE)
    if kv_id != 0 {
        return kv_id
    }

    kv_count_effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.kv_count_obs(1))
    if service_effect.effect_reply_status(kv_count_effect) != syscall.SyscallStatus.Ok {
        return FAIL_RESTART_KV_COUNT_STATUS
    }
    if service_effect.effect_reply_payload_len(kv_count_effect) != 4 {
        return FAIL_RESTART_KV_COUNT_LEN
    }

    kv_effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_kv_get(20))
    if service_effect.effect_reply_status(kv_effect) != syscall.SyscallStatus.Ok {
        return FAIL_RESTART_KV_GET_STATUS
    }
    if service_effect.effect_reply_payload_len(kv_effect) != 2 {
        return FAIL_RESTART_KV_GET_LEN
    }
    kv_payload: [4]u8 = service_effect.effect_reply_payload(kv_effect)
    if kv_payload[0] != 20 {
        return FAIL_RESTART_KV_GET_KEY
    }
    if kv_payload[1] != 77 {
        return FAIL_RESTART_KV_GET_VALUE
    }

    kv_overwrite_effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_kv_set(20, 88))
    if service_effect.effect_reply_status(kv_overwrite_effect) != syscall.SyscallStatus.Ok {
        return FAIL_RESTART_KV_OVERWRITE_STATUS
    }

    kv_effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_kv_get(20))
    if service_effect.effect_reply_status(kv_effect) != syscall.SyscallStatus.Ok {
        return FAIL_RESTART_KV_SECOND_GET_STATUS
    }
    if service_effect.effect_reply_payload(kv_effect)[1] != 88 {
        return FAIL_RESTART_KV_SECOND_GET_VALUE
    }

    queue_before: service_identity.ServiceMark = boot.boot_queue_mark(*state)
    queue_effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_queue_enqueue(61))
    if service_effect.effect_reply_status(queue_effect) != syscall.SyscallStatus.Ok {
        return FAIL_RESTART_QUEUE_ENQUEUE_0
    }
    queue_effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_queue_enqueue(62))
    if service_effect.effect_reply_status(queue_effect) != syscall.SyscallStatus.Ok {
        return FAIL_RESTART_QUEUE_ENQUEUE_1
    }
    queue_effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_queue_enqueue(63))
    if service_effect.effect_reply_status(queue_effect) != syscall.SyscallStatus.Ok {
        return FAIL_RESTART_QUEUE_ENQUEUE_2
    }

    *state = init.restart(*state, service_topology.QUEUE_ENDPOINT_ID)
    queue_after: service_identity.ServiceMark = boot.boot_queue_mark(*state)

    queue_before_endpoint: u32 = service_identity.mark_endpoint(queue_before)
    queue_before_pid: u32 = service_identity.mark_pid(queue_before)
    queue_before_generation: u32 = service_identity.mark_generation(queue_before)
    queue_after_endpoint: u32 = service_identity.mark_endpoint(queue_after)
    queue_after_pid: u32 = service_identity.mark_pid(queue_after)
    queue_after_generation: u32 = service_identity.mark_generation(queue_after)
    queue_id: i32 = scenario_assert.expect_restart_identity(queue_before_endpoint, queue_before_pid, queue_before_generation, queue_after_endpoint, queue_after_pid, queue_after_generation, FAIL_RESTART_QUEUE_IDENTITY_BASE)
    if queue_id != 0 {
        return queue_id
    }

    queue_effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_queue_dequeue())
    if service_effect.effect_reply_status(queue_effect) != syscall.SyscallStatus.Ok {
        return FAIL_RESTART_QUEUE_DEQUEUE_0_STATUS
    }
    if service_effect.effect_reply_payload(queue_effect)[0] != 61 {
        return FAIL_RESTART_QUEUE_DEQUEUE_0_VALUE
    }

    queue_effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_queue_dequeue())
    if service_effect.effect_reply_status(queue_effect) != syscall.SyscallStatus.Ok {
        return FAIL_RESTART_QUEUE_DEQUEUE_1_STATUS
    }
    if service_effect.effect_reply_payload(queue_effect)[0] != 62 {
        return FAIL_RESTART_QUEUE_DEQUEUE_1_VALUE
    }

    queue_effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_queue_enqueue(64))
    if service_effect.effect_reply_status(queue_effect) != syscall.SyscallStatus.Ok {
        return FAIL_RESTART_QUEUE_ENQUEUE_3_STATUS
    }

    queue_effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_queue_dequeue())
    if service_effect.effect_reply_status(queue_effect) != syscall.SyscallStatus.Ok {
        return FAIL_RESTART_QUEUE_DEQUEUE_2_STATUS
    }
    if service_effect.effect_reply_payload(queue_effect)[0] != 63 {
        return FAIL_RESTART_QUEUE_DEQUEUE_2_VALUE
    }

    queue_effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_queue_dequeue())
    if service_effect.effect_reply_status(queue_effect) != syscall.SyscallStatus.Ok {
        return FAIL_RESTART_QUEUE_DEQUEUE_3_STATUS
    }
    if service_effect.effect_reply_payload(queue_effect)[0] != 64 {
        return FAIL_RESTART_QUEUE_DEQUEUE_3_VALUE
    }

    queue_effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_queue_dequeue())
    if service_effect.effect_reply_status(queue_effect) != syscall.SyscallStatus.InvalidArgument {
        return FAIL_RESTART_QUEUE_EMPTY_STATUS
    }

    echo_before: service_identity.ServiceMark = boot.boot_echo_mark(*state)
    *state = init.restart(*state, service_topology.ECHO_ENDPOINT_ID)
    echo_after: service_identity.ServiceMark = boot.boot_echo_mark(*state)

    echo_before_endpoint: u32 = service_identity.mark_endpoint(echo_before)
    echo_before_pid: u32 = service_identity.mark_pid(echo_before)
    echo_before_generation: u32 = service_identity.mark_generation(echo_before)
    echo_after_endpoint: u32 = service_identity.mark_endpoint(echo_after)
    echo_after_pid: u32 = service_identity.mark_pid(echo_after)
    echo_after_generation: u32 = service_identity.mark_generation(echo_after)
    echo_id: i32 = scenario_assert.expect_restart_identity(echo_before_endpoint, echo_before_pid, echo_before_generation, echo_after_endpoint, echo_after_pid, echo_after_generation, FAIL_RESTART_ECHO_IDENTITY_BASE)
    if echo_id != 0 {
        return echo_id
    }

    echo_effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_echo(33, 44))
    if service_effect.effect_reply_status(echo_effect) != syscall.SyscallStatus.Ok {
        return FAIL_RESTART_ECHO_STATUS
    }
    if service_effect.effect_reply_payload_len(echo_effect) != 2 {
        return FAIL_RESTART_ECHO_LEN
    }
    if service_effect.effect_reply_payload(echo_effect)[0] != 33 {
        return FAIL_RESTART_ECHO_PAYLOAD_0
    }
    if service_effect.effect_reply_payload(echo_effect)[1] != 44 {
        return FAIL_RESTART_ECHO_PAYLOAD_1
    }

    ticket_before: service_identity.ServiceMark = boot.boot_ticket_mark(*state)
    ticket_issue_effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_ticket_issue())
    if service_effect.effect_reply_status(ticket_issue_effect) != syscall.SyscallStatus.Ok {
        return FAIL_RESTART_TICKET_ISSUE_STATUS
    }
    if service_effect.effect_reply_payload_len(ticket_issue_effect) != 2 {
        return FAIL_RESTART_TICKET_ISSUE_LEN
    }
    stale_ticket: [4]u8 = service_effect.effect_reply_payload(ticket_issue_effect)

    *state = init.restart(*state, service_topology.TICKET_ENDPOINT_ID)
    ticket_after: service_identity.ServiceMark = boot.boot_ticket_mark(*state)

    ticket_before_endpoint: u32 = service_identity.mark_endpoint(ticket_before)
    ticket_before_pid: u32 = service_identity.mark_pid(ticket_before)
    ticket_before_generation: u32 = service_identity.mark_generation(ticket_before)
    ticket_after_endpoint: u32 = service_identity.mark_endpoint(ticket_after)
    ticket_after_pid: u32 = service_identity.mark_pid(ticket_after)
    ticket_after_generation: u32 = service_identity.mark_generation(ticket_after)
    ticket_id: i32 = scenario_assert.expect_restart_identity(ticket_before_endpoint, ticket_before_pid, ticket_before_generation, ticket_after_endpoint, ticket_after_pid, ticket_after_generation, FAIL_RESTART_TICKET_IDENTITY_BASE)
    if ticket_id != 0 {
        return ticket_id
    }

    ticket_use_effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_ticket_use(stale_ticket[0], stale_ticket[1]))
    if service_effect.effect_reply_status(ticket_use_effect) != syscall.SyscallStatus.InvalidArgument {
        return FAIL_RESTART_TICKET_STALE_STATUS
    }
    if service_effect.effect_reply_payload_len(ticket_use_effect) != 1 {
        return FAIL_RESTART_TICKET_STALE_LEN
    }
    if service_effect.effect_reply_payload(ticket_use_effect)[0] != ticket_service.TICKET_STALE {
        return FAIL_RESTART_TICKET_STALE_KIND
    }

    ticket_issue_effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_ticket_issue())
    if service_effect.effect_reply_status(ticket_issue_effect) != syscall.SyscallStatus.Ok {
        return FAIL_RESTART_TICKET_FRESH_ISSUE_STATUS
    }
    if service_effect.effect_reply_payload_len(ticket_issue_effect) != 2 {
        return FAIL_RESTART_TICKET_FRESH_ISSUE_LEN
    }
    fresh_ticket: [4]u8 = service_effect.effect_reply_payload(ticket_issue_effect)
    if fresh_ticket[0] == stale_ticket[0] {
        return FAIL_RESTART_TICKET_EPOCH_REUSED
    }

    ticket_use_effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_ticket_use(fresh_ticket[0], fresh_ticket[1]))
    if service_effect.effect_reply_status(ticket_use_effect) != syscall.SyscallStatus.Ok {
        return FAIL_RESTART_TICKET_USE_STATUS
    }
    if service_effect.effect_reply_payload_len(ticket_use_effect) != 1 {
        return FAIL_RESTART_TICKET_USE_LEN
    }
    if service_effect.effect_reply_payload(ticket_use_effect)[0] != fresh_ticket[1] {
        return FAIL_RESTART_TICKET_USE_VALUE
    }

    ticket_use_effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_ticket_use(fresh_ticket[0], fresh_ticket[1]))
    if service_effect.effect_reply_status(ticket_use_effect) != syscall.SyscallStatus.InvalidArgument {
        return FAIL_RESTART_TICKET_INVALID_STATUS
    }
    if service_effect.effect_reply_payload_len(ticket_use_effect) != 1 {
        return FAIL_RESTART_TICKET_INVALID_LEN
    }
    if service_effect.effect_reply_payload(ticket_use_effect)[0] != ticket_service.TICKET_INVALID {
        return FAIL_RESTART_TICKET_INVALID_KIND
    }

    return 0
}