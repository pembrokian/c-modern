// Phase 185: admit one explicit transferred-handle runtime path.
//
// Ordinary named traffic still rejects handle metadata at the gateway.
// The only admitted exception is the transfer endpoint, which consumes one
// grant token destructively and uses it for one bounded send.

import boot
import init
import ipc
import kernel_dispatch
import service_effect
import service_identity
import service_topology
import syscall
import transfer_grant

struct GrantStep {
    state: boot.KernelBootState
    slot: u32
    status: syscall.SyscallStatus
}

func issue_log_grant(state: boot.KernelBootState, pid: u32) GrantStep {
    issued: transfer_grant.GrantIssue = transfer_grant.grant_issue(state.grants, pid, service_topology.LOG_ENDPOINT_ID)
    return GrantStep{ state: boot.bootwith_grants(state, issued.table), slot: issued.slot, status: issued.status }
}

func obs(endpoint: u32, pid: u32, payload_len: usize, slot: u32, count: usize, payload: [4]u8) syscall.ReceiveObservation {
    return syscall.ReceiveObservation{ status: syscall.SyscallStatus.Ok, block_reason: syscall.BlockReason.None, endpoint_id: endpoint, source_pid: pid, payload_len: payload_len, received_handle_slot: slot, received_handle_count: count, payload: payload }
}

func one_byte(value: u8) [4]u8 {
    payload: [4]u8 = ipc.zero_payload()
    payload[0] = value
    return payload
}

func rejected(effect: service_effect.Effect) bool {
    return service_effect.effect_reply_status(effect) == syscall.SyscallStatus.InvalidCapability
}

func check_named_path_still_rejects_handle_metadata() bool {
    state: boot.KernelBootState = boot.kernel_init()
    granted: GrantStep = issue_log_grant(state, 41)
    if granted.status != syscall.SyscallStatus.Ok {
        return false
    }
    effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(&granted.state, obs(service_topology.LOG_ENDPOINT_ID, 41, 1, granted.slot, 1, one_byte(7)))
    return rejected(effect)
}

func check_transfer_consumes_grant() bool {
    state: boot.KernelBootState = boot.kernel_init()
    granted: GrantStep = issue_log_grant(state, 41)
    if granted.status != syscall.SyscallStatus.Ok {
        return false
    }
    state = granted.state

    effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(&state, obs(service_topology.TRANSFER_ENDPOINT_ID, 41, 1, granted.slot, 1, one_byte(77)))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    tail: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(&state, obs(service_topology.LOG_ENDPOINT_ID, 41, 0, 0, 0, ipc.zero_payload()))
    if service_effect.effect_reply_status(tail) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload_len(tail) != 1 {
        return false
    }
    if service_effect.effect_reply_payload(tail)[0] != 77 {
        return false
    }

    stale: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(&state, obs(service_topology.TRANSFER_ENDPOINT_ID, 41, 1, granted.slot, 1, one_byte(99)))
    return rejected(stale)
}

func check_restart_keeps_route_but_not_instance() bool {
    state: boot.KernelBootState = boot.kernel_init()
    before: service_identity.ServiceMark = boot.boot_log_mark(state)
    state = init.restart(state, service_topology.LOG_ENDPOINT_ID)
    after: service_identity.ServiceMark = boot.boot_log_mark(state)

    if !service_identity.marks_same_endpoint(before, after) {
        return false
    }
    if !service_identity.marks_same_pid(before, after) {
        return false
    }
    if service_identity.marks_same_instance(before, after) {
        return false
    }

    granted: GrantStep = issue_log_grant(state, 52)
    if granted.status != syscall.SyscallStatus.Ok {
        return false
    }
    state = granted.state

    effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(&state, obs(service_topology.TRANSFER_ENDPOINT_ID, 52, 1, granted.slot, 1, one_byte(88)))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    state = init.restart(state, service_topology.LOG_ENDPOINT_ID)
    tail: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(&state, obs(service_topology.LOG_ENDPOINT_ID, 52, 0, 0, 0, ipc.zero_payload()))
    if service_effect.effect_reply_status(tail) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload_len(tail) != 1 {
        return false
    }
    if service_effect.effect_reply_payload(tail)[0] != 88 {
        return false
    }

    stale: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(&state, obs(service_topology.TRANSFER_ENDPOINT_ID, 52, 1, granted.slot, 1, one_byte(44)))
    return rejected(stale)
}

func main() i32 {
    if !check_named_path_still_rejects_handle_metadata() {
        return 1
    }
    if !check_transfer_consumes_grant() {
        return 2
    }
    if !check_restart_keeps_route_but_not_instance() {
        return 3
    }
    return 0
}