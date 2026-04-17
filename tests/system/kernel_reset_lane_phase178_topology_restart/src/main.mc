import boot
import init
import kernel_dispatch
import serial_protocol
import service_effect
import service_topology
import syscall

func cmd(payload: [4]u8) syscall.ReceiveObservation {
    return syscall.ReceiveObservation{ status: syscall.SyscallStatus.Ok, block_reason: syscall.BlockReason.None, endpoint_id: service_topology.SERIAL_ENDPOINT_ID, source_pid: 1, payload_len: 4, received_handle_slot: 0, received_handle_count: 0, payload: payload }
}

func smoke_topology_is_enumerable() bool {
    expected: [17]u32
    expected[0] = service_topology.SERIAL_ENDPOINT_ID
    expected[1] = service_topology.SHELL_ENDPOINT_ID
    expected[2] = service_topology.LOG_ENDPOINT_ID
    expected[3] = service_topology.KV_ENDPOINT_ID
    expected[4] = service_topology.ECHO_ENDPOINT_ID
    expected[5] = service_topology.TRANSFER_ENDPOINT_ID
    expected[6] = service_topology.QUEUE_ENDPOINT_ID
    expected[7] = service_topology.TICKET_ENDPOINT_ID
    expected[8] = service_topology.FILE_ENDPOINT_ID
    expected[9] = service_topology.TIMER_ENDPOINT_ID
    expected[10] = service_topology.TASK_ENDPOINT_ID
    expected[11] = service_topology.JOURNAL_ENDPOINT_ID
    expected[12] = service_topology.WORKFLOW_ENDPOINT_ID
    expected[13] = service_topology.LEASE_ENDPOINT_ID
    expected[14] = service_topology.COMPLETION_MAILBOX_ENDPOINT_ID
    expected[15] = service_topology.OBJECT_STORE_ENDPOINT_ID
    expected[16] = service_topology.CONNECTION_ENDPOINT_ID

    if service_topology.service_count() != 17 {
        return false
    }
    for i in 0..service_topology.service_count() {
        slot: service_topology.ServiceSlot = service_topology.service_slot_at(i)
        if !service_topology.service_slot_is_valid(slot) {
            return false
        }
        if slot.endpoint != expected[i] {
            return false
        }
    }

    invalid: service_topology.ServiceSlot = service_topology.service_slot_at(17)
    if service_topology.service_slot_is_valid(invalid) {
        return false
    }

    echo_slot: service_topology.ServiceSlot = service_topology.service_slot_for_endpoint(service_topology.ECHO_ENDPOINT_ID)
    if !service_topology.service_slot_is_valid(echo_slot) {
        return false
    }
    if echo_slot.pid != 5 {
        return false
    }
    return true
}

func smoke_lifecycle_classification_stays_flat() bool {
    if service_topology.service_can_restart(service_topology.SERIAL_ENDPOINT_ID) {
        return false
    }
    if service_topology.service_can_restart(service_topology.SHELL_ENDPOINT_ID) {
        return false
    }
    if !service_topology.service_can_restart(service_topology.LOG_ENDPOINT_ID) {
        return false
    }
    if !service_topology.service_restart_reloads_state(service_topology.LOG_ENDPOINT_ID) {
        return false
    }
    if !service_topology.service_can_restart(service_topology.KV_ENDPOINT_ID) {
        return false
    }
    if !service_topology.service_restart_reloads_state(service_topology.KV_ENDPOINT_ID) {
        return false
    }
    if !service_topology.service_can_restart(service_topology.ECHO_ENDPOINT_ID) {
        return false
    }
    if service_topology.service_restart_reloads_state(service_topology.ECHO_ENDPOINT_ID) {
        return false
    }
    if !service_topology.service_can_restart(service_topology.TRANSFER_ENDPOINT_ID) {
        return false
    }
    if service_topology.service_restart_reloads_state(service_topology.TRANSFER_ENDPOINT_ID) {
        return false
    }
    if !service_topology.service_can_restart(service_topology.QUEUE_ENDPOINT_ID) {
        return false
    }
    if !service_topology.service_restart_reloads_state(service_topology.QUEUE_ENDPOINT_ID) {
        return false
    }
    if !service_topology.service_can_restart(service_topology.TICKET_ENDPOINT_ID) {
        return false
    }
    if service_topology.service_restart_reloads_state(service_topology.TICKET_ENDPOINT_ID) {
        return false
    }
    if !service_topology.service_can_restart(service_topology.FILE_ENDPOINT_ID) {
        return false
    }
    if !service_topology.service_restart_reloads_state(service_topology.FILE_ENDPOINT_ID) {
        return false
    }
    if !service_topology.service_can_restart(service_topology.TIMER_ENDPOINT_ID) {
        return false
    }
    if !service_topology.service_restart_reloads_state(service_topology.TIMER_ENDPOINT_ID) {
        return false
    }
    if !service_topology.service_can_restart(service_topology.TASK_ENDPOINT_ID) {
        return false
    }
    if service_topology.service_restart_reloads_state(service_topology.TASK_ENDPOINT_ID) {
        return false
    }
    if !service_topology.service_can_restart(service_topology.CONNECTION_ENDPOINT_ID) {
        return false
    }
    if service_topology.service_restart_reloads_state(service_topology.CONNECTION_ENDPOINT_ID) {
        return false
    }
    if !service_topology.service_can_restart(service_topology.JOURNAL_ENDPOINT_ID) {
        return false
    }
    if !service_topology.service_restart_reloads_state(service_topology.JOURNAL_ENDPOINT_ID) {
        return false
    }
    if !service_topology.service_can_restart(service_topology.WORKFLOW_ENDPOINT_ID) {
        return false
    }
    if !service_topology.service_restart_reloads_state(service_topology.WORKFLOW_ENDPOINT_ID) {
        return false
    }
    if !service_topology.service_can_restart(service_topology.LEASE_ENDPOINT_ID) {
        return false
    }
    if service_topology.service_restart_reloads_state(service_topology.LEASE_ENDPOINT_ID) {
        return false
    }
    if !service_topology.service_can_restart(service_topology.COMPLETION_MAILBOX_ENDPOINT_ID) {
        return false
    }
    if !service_topology.service_restart_reloads_state(service_topology.COMPLETION_MAILBOX_ENDPOINT_ID) {
        return false
    }
    if !service_topology.service_can_restart(service_topology.OBJECT_STORE_ENDPOINT_ID) {
        return false
    }
    if !service_topology.service_restart_reloads_state(service_topology.OBJECT_STORE_ENDPOINT_ID) {
        return false
    }
    return true
}

func smoke_endpoint_restart_recovers_echo() bool {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_echo(1, 2)))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_echo(3, 4)))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_echo(5, 6)))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_echo(7, 8)))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_echo(9, 10)))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Exhausted {
        return false
    }

    state = init.restart(state, service_topology.ECHO_ENDPOINT_ID)
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_echo(11, 12)))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload(effect)[0] != 11 {
        return false
    }
    if service_effect.effect_reply_payload(effect)[1] != 12 {
        return false
    }
    return true
}

func main() i32 {
    if !smoke_topology_is_enumerable() {
        return 1
    }
    if !smoke_lifecycle_classification_stays_flat() {
        return 2
    }
    if !smoke_endpoint_restart_recovers_echo() {
        return 3
    }
    return 0
}