import boot
import init
import kernel_dispatch
import serial_protocol
import service_effect
import service_identity
import service_topology
import syscall
import ticket_service

func cmd(payload: [4]u8) syscall.ReceiveObservation {
    return syscall.ReceiveObservation{ status: syscall.SyscallStatus.Ok, block_reason: syscall.BlockReason.None, endpoint_id: service_topology.SERIAL_ENDPOINT_ID, source_pid: 1, payload_len: 4, received_handle_slot: 0, received_handle_count: 0, payload: payload }
}

func issue(state: *boot.KernelBootState) service_effect.Effect {
    return kernel_dispatch.kernel_dispatch_step(state, cmd(serial_protocol.encode_ticket_issue()))
}

func use(state: *boot.KernelBootState, epoch: u8, id: u8) service_effect.Effect {
    return kernel_dispatch.kernel_dispatch_step(state, cmd(serial_protocol.encode_ticket_use(epoch, id)))
}

func smoke_ticket_restart_is_visible() bool {
    state: boot.KernelBootState = boot.kernel_init()
    before: service_identity.ServiceMark = boot.boot_ticket_mark(state)

    issued: service_effect.Effect = issue(&state)
    if service_effect.effect_reply_status(issued) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload_len(issued) != 2 {
        return false
    }
    stale_ticket: [4]u8 = service_effect.effect_reply_payload(issued)

    state = init.restart(state, service_topology.TICKET_ENDPOINT_ID)
    after: service_identity.ServiceMark = boot.boot_ticket_mark(state)

    if !service_identity.marks_same_endpoint(before, after) {
        return false
    }
    if !service_identity.marks_same_pid(before, after) {
        return false
    }
    if service_identity.marks_same_instance(before, after) {
        return false
    }

    stale: service_effect.Effect = use(&state, stale_ticket[0], stale_ticket[1])
    if service_effect.effect_reply_status(stale) != syscall.SyscallStatus.InvalidArgument {
        return false
    }
    if service_effect.effect_reply_payload_len(stale) != 1 {
        return false
    }
    if service_effect.effect_reply_payload(stale)[0] != ticket_service.TICKET_STALE {
        return false
    }

    fresh_issue: service_effect.Effect = issue(&state)
    if service_effect.effect_reply_status(fresh_issue) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload_len(fresh_issue) != 2 {
        return false
    }
    fresh_ticket: [4]u8 = service_effect.effect_reply_payload(fresh_issue)
    if fresh_ticket[0] == stale_ticket[0] {
        return false
    }

    consumed: service_effect.Effect = use(&state, fresh_ticket[0], fresh_ticket[1])
    if service_effect.effect_reply_status(consumed) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload_len(consumed) != 1 {
        return false
    }
    if service_effect.effect_reply_payload(consumed)[0] != fresh_ticket[1] {
        return false
    }

    invalid: service_effect.Effect = use(&state, fresh_ticket[0], fresh_ticket[1])
    if service_effect.effect_reply_status(invalid) != syscall.SyscallStatus.InvalidArgument {
        return false
    }
    if service_effect.effect_reply_payload_len(invalid) != 1 {
        return false
    }
    if service_effect.effect_reply_payload(invalid)[0] != ticket_service.TICKET_INVALID {
        return false
    }

    return true
}

func main() i32 {
    if !smoke_ticket_restart_is_visible() {
        return 1
    }
    return 0
}