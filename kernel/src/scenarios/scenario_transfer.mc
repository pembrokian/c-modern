// Multi-handle transfer follow-through probe.

import boot
import init
import kernel_dispatch
import scenario_assert
import scenario_transport
import service_effect
import service_identity
import service_topology
import syscall
import transfer_grant
import transfer_service

const FAIL_TRANSFER_PAIR_ISSUE_STATUS: i32 = 79
const FAIL_TRANSFER_PAIR_ISSUE_CONTIGUOUS: i32 = 80
const FAIL_TRANSFER_BIND_STATUS: i32 = 81
const FAIL_TRANSFER_LOG_STATUS: i32 = 82
const FAIL_TRANSFER_LOG_TAIL_STATUS: i32 = 83
const FAIL_TRANSFER_LOG_TAIL_LEN: i32 = 84
const FAIL_TRANSFER_LOG_TAIL_VALUE: i32 = 85
const FAIL_TRANSFER_KV_STATUS: i32 = 86
const FAIL_TRANSFER_KV_GET_STATUS: i32 = 87
const FAIL_TRANSFER_KV_GET_LEN: i32 = 88
const FAIL_TRANSFER_KV_GET_VALUE: i32 = 89
const FAIL_TRANSFER_STALE_STATUS: i32 = 90
const FAIL_TRANSFER_REUSE_STATUS: i32 = 91
const FAIL_TRANSFER_REUSE_DISTINCT: i32 = 92
const FAIL_TRANSFER_OLD_REUSED_STATUS: i32 = 93
const FAIL_TRANSFER_IDENTITY_BASE: i32 = 94
const FAIL_TRANSFER_RESTART_LOST_STATUS: i32 = 98
const FAIL_TRANSFER_RESTART_STALE_STATUS: i32 = 99
const FAIL_TRANSFER_RESTART_REBIND_STATUS: i32 = 100
const FAIL_TRANSFER_RESTART_LOG_STATUS: i32 = 101
const FAIL_TRANSFER_RESTART_LOG_TAIL_STATUS: i32 = 102
const FAIL_TRANSFER_RESTART_LOG_TAIL_LEN: i32 = 103
const FAIL_TRANSFER_RESTART_LOG_TAIL_VALUE: i32 = 104

struct GrantPair {
    state: boot.KernelBootState
    slot: u32
    status: syscall.SyscallStatus
}

func transfer_obs(pid: u32, payload_len: usize, slot: u32, count: usize, payload: [4]u8) syscall.ReceiveObservation {
    return syscall.ReceiveObservation{ status: syscall.SyscallStatus.Ok, block_reason: syscall.BlockReason.None, endpoint_id: service_topology.TRANSFER_ENDPOINT_ID, source_pid: pid, payload_len: payload_len, received_handle_slot: slot, received_handle_count: count, payload: payload }
}

func bind_payload() [4]u8 {
    payload: [4]u8 = scenario_transport.cmd_log_tail().payload
    payload[0] = transfer_service.TRANSFER_CMD_BIND
    payload[1] = 0
    payload[2] = 0
    payload[3] = 0
    return payload
}

func log_payload(value: u8) [4]u8 {
    payload: [4]u8 = scenario_transport.cmd_log_tail().payload
    payload[0] = transfer_service.TRANSFER_CMD_LOG
    payload[1] = value
    payload[2] = 0
    payload[3] = 0
    return payload
}

func kv_payload(key: u8, value: u8) [4]u8 {
    payload: [4]u8 = scenario_transport.cmd_log_tail().payload
    payload[0] = transfer_service.TRANSFER_CMD_KV
    payload[1] = key
    payload[2] = value
    payload[3] = 0
    return payload
}

func issue_pair(state: boot.KernelBootState, pid: u32) GrantPair {
    first: transfer_grant.GrantIssue = transfer_grant.grant_issue(state.grants, pid, service_topology.LOG_ENDPOINT_ID)
    next: boot.KernelBootState = boot.bootwith_grants(state, first.table)
    if first.status != syscall.SyscallStatus.Ok {
        return GrantPair{ state: next, slot: 0, status: first.status }
    }
    second: transfer_grant.GrantIssue = transfer_grant.grant_issue(next.grants, pid, service_topology.KV_ENDPOINT_ID)
    next = boot.bootwith_grants(next, second.table)
    if second.status != syscall.SyscallStatus.Ok {
        return GrantPair{ state: next, slot: first.slot, status: second.status }
    }
    if second.slot != first.slot + 1 {
        return GrantPair{ state: next, slot: first.slot, status: syscall.SyscallStatus.InvalidCapability }
    }
    return GrantPair{ state: next, slot: first.slot, status: syscall.SyscallStatus.Ok }
}

func run_transfer_probe(state: *boot.KernelBootState) i32 {
    pair: GrantPair = issue_pair(*state, 41)
    if pair.status != syscall.SyscallStatus.Ok {
        if pair.status == syscall.SyscallStatus.InvalidCapability {
            return FAIL_TRANSFER_PAIR_ISSUE_CONTIGUOUS
        }
        return FAIL_TRANSFER_PAIR_ISSUE_STATUS
    }
    *state = pair.state

    bind_effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(state, transfer_obs(41, 1, pair.slot, 2, bind_payload()))
    if service_effect.effect_reply_status(bind_effect) != syscall.SyscallStatus.Ok {
        return FAIL_TRANSFER_BIND_STATUS
    }

    log_effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(state, transfer_obs(41, 2, 0, 0, log_payload(81)))
    if service_effect.effect_reply_status(log_effect) != syscall.SyscallStatus.Ok {
        return FAIL_TRANSFER_LOG_STATUS
    }

    tail: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_log_tail())
    if service_effect.effect_reply_status(tail) != syscall.SyscallStatus.Ok {
        return FAIL_TRANSFER_LOG_TAIL_STATUS
    }
    if service_effect.effect_reply_payload_len(tail) != 1 {
        return FAIL_TRANSFER_LOG_TAIL_LEN
    }
    if service_effect.effect_reply_payload(tail)[0] != 81 {
        return FAIL_TRANSFER_LOG_TAIL_VALUE
    }

    kv_effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(state, transfer_obs(41, 3, 0, 0, kv_payload(9, 44)))
    if service_effect.effect_reply_status(kv_effect) != syscall.SyscallStatus.Ok {
        return FAIL_TRANSFER_KV_STATUS
    }

    get_effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_kv_get(9))
    if service_effect.effect_reply_status(get_effect) != syscall.SyscallStatus.Ok {
        return FAIL_TRANSFER_KV_GET_STATUS
    }
    if service_effect.effect_reply_payload_len(get_effect) != 2 {
        return FAIL_TRANSFER_KV_GET_LEN
    }
    if service_effect.effect_reply_payload(get_effect)[1] != 44 {
        return FAIL_TRANSFER_KV_GET_VALUE
    }

    stale_effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(state, transfer_obs(41, 1, pair.slot, 2, bind_payload()))
    if service_effect.effect_reply_status(stale_effect) != syscall.SyscallStatus.InvalidCapability {
        return FAIL_TRANSFER_STALE_STATUS
    }

    reused: GrantPair = issue_pair(*state, 41)
    if reused.status != syscall.SyscallStatus.Ok {
        return FAIL_TRANSFER_REUSE_STATUS
    }
    if reused.slot == pair.slot {
        return FAIL_TRANSFER_REUSE_DISTINCT
    }
    *state = reused.state

    stale_effect = kernel_dispatch.kernel_dispatch_step(state, transfer_obs(41, 1, pair.slot, 2, bind_payload()))
    if service_effect.effect_reply_status(stale_effect) != syscall.SyscallStatus.InvalidCapability {
        return FAIL_TRANSFER_OLD_REUSED_STATUS
    }

    bind_effect = kernel_dispatch.kernel_dispatch_step(state, transfer_obs(41, 1, reused.slot, 2, bind_payload()))
    if service_effect.effect_reply_status(bind_effect) != syscall.SyscallStatus.Ok {
        return FAIL_TRANSFER_BIND_STATUS
    }

    before: service_identity.ServiceMark = boot.boot_transfer_mark(*state)
    *state = init.restart(*state, service_topology.TRANSFER_ENDPOINT_ID)
    after: service_identity.ServiceMark = boot.boot_transfer_mark(*state)
    before_endpoint: u32 = service_identity.mark_endpoint(before)
    before_pid: u32 = service_identity.mark_pid(before)
    before_generation: u32 = service_identity.mark_generation(before)
    after_endpoint: u32 = service_identity.mark_endpoint(after)
    after_pid: u32 = service_identity.mark_pid(after)
    after_generation: u32 = service_identity.mark_generation(after)
    restart_id: i32 = scenario_assert.expect_restart_identity(before_endpoint, before_pid, before_generation, after_endpoint, after_pid, after_generation, FAIL_TRANSFER_IDENTITY_BASE)
    if restart_id != 0 {
        return restart_id
    }

    lost_effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(state, transfer_obs(41, 2, 0, 0, log_payload(99)))
    if service_effect.effect_reply_status(lost_effect) != syscall.SyscallStatus.InvalidCapability {
        return FAIL_TRANSFER_RESTART_LOST_STATUS
    }

    stale_effect = kernel_dispatch.kernel_dispatch_step(state, transfer_obs(41, 1, reused.slot, 2, bind_payload()))
    if service_effect.effect_reply_status(stale_effect) != syscall.SyscallStatus.InvalidCapability {
        return FAIL_TRANSFER_RESTART_STALE_STATUS
    }

    reused = issue_pair(*state, 41)
    if reused.status != syscall.SyscallStatus.Ok {
        return FAIL_TRANSFER_REUSE_STATUS
    }
    *state = reused.state
    bind_effect = kernel_dispatch.kernel_dispatch_step(state, transfer_obs(41, 1, reused.slot, 2, bind_payload()))
    if service_effect.effect_reply_status(bind_effect) != syscall.SyscallStatus.Ok {
        return FAIL_TRANSFER_RESTART_REBIND_STATUS
    }

    log_effect = kernel_dispatch.kernel_dispatch_step(state, transfer_obs(41, 2, 0, 0, log_payload(99)))
    if service_effect.effect_reply_status(log_effect) != syscall.SyscallStatus.Ok {
        return FAIL_TRANSFER_RESTART_LOG_STATUS
    }

    tail = kernel_dispatch.kernel_dispatch_step(state, scenario_transport.cmd_log_tail())
    if service_effect.effect_reply_status(tail) != syscall.SyscallStatus.Ok {
        return FAIL_TRANSFER_RESTART_LOG_TAIL_STATUS
    }
    if service_effect.effect_reply_payload_len(tail) != 3 {
        return FAIL_TRANSFER_RESTART_LOG_TAIL_LEN
    }
    if service_effect.effect_reply_payload(tail)[2] != 99 {
        return FAIL_TRANSFER_RESTART_LOG_TAIL_VALUE
    }

    return 0
}