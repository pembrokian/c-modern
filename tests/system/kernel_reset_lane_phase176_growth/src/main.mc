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

func smoke_echo_service_is_named() bool {
    echo_slot: service_topology.ServiceSlot = service_topology.ECHO_SLOT
    if service_topology.SERVICE_COUNT != 14 {
        return false
    }
    if echo_slot.endpoint != service_topology.ECHO_ENDPOINT_ID {
        return false
    }
    if echo_slot.pid != 5 {
        return false
    }
    return true
}

func smoke_echo_route_round_trip() bool {
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_echo(12, 34)))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 2 {
        return false
    }
    if service_effect.effect_reply_payload(effect)[0] != 12 {
        return false
    }
    if service_effect.effect_reply_payload(effect)[1] != 34 {
        return false
    }
    return true
}

func smoke_echo_pressure_and_restart() bool {
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

    state = init.restart_echo(state)
    effect = kernel_dispatch.kernel_dispatch_step(&state, cmd(serial_protocol.encode_echo(21, 22)))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload(effect)[0] != 21 {
        return false
    }
    if service_effect.effect_reply_payload(effect)[1] != 22 {
        return false
    }

    return true
}

func main() i32 {
    if smoke_echo_service_is_named() == false {
        return 1
    }
    if smoke_echo_route_round_trip() == false {
        return 2
    }
    if smoke_echo_pressure_and_restart() == false {
        return 3
    }
    return 0
}