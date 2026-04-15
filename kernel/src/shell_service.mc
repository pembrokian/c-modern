import primitives
import serial_protocol
import service_effect
import service_topology
import syscall

const SHELL_INVALID_REPLY: u8 = 63  // '?'

struct ShellServiceState {
    pid: u32
    slot: u32
}

func shell_init(pid: u32, slot: u32) ShellServiceState {
    return ShellServiceState{ pid: pid, slot: slot }
}

func invalid_effect() service_effect.Effect {
    payload: [4]u8 = primitives.zero_payload()
    payload[0] = SHELL_INVALID_REPLY
    return service_effect.effect_reply(syscall.SyscallStatus.InvalidArgument, 1, payload)
}

func handle(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    payload: [4]u8 = primitives.zero_payload()

    if s.pid == 0 || m.payload_len != 4 {
        return invalid_effect()
    }

    if m.payload[0] == serial_protocol.CMD_E && m.payload[1] == serial_protocol.CMD_C {
        payload[0] = m.payload[2]
        payload[1] = m.payload[3]
        return service_effect.effect_send(s.pid, service_topology.ECHO_ENDPOINT_ID, 2, payload)
    }

    if m.payload[0] == serial_protocol.CMD_L && m.payload[1] == serial_protocol.CMD_A && m.payload[3] == serial_protocol.CMD_BANG {
        payload[0] = m.payload[2]
        return service_effect.effect_send(s.pid, service_topology.LOG_ENDPOINT_ID, 1, payload)
    }

    if m.payload[0] == serial_protocol.CMD_L && m.payload[1] == serial_protocol.CMD_T && m.payload[2] == serial_protocol.CMD_BANG && m.payload[3] == serial_protocol.CMD_BANG {
        return service_effect.effect_send(s.pid, service_topology.LOG_ENDPOINT_ID, 0, payload)
    }

    if m.payload[0] == serial_protocol.CMD_K && m.payload[1] == serial_protocol.CMD_S {
        payload[0] = m.payload[2]
        payload[1] = m.payload[3]
        return service_effect.effect_send(s.pid, service_topology.KV_ENDPOINT_ID, 2, payload)
    }

    if m.payload[0] == serial_protocol.CMD_K && m.payload[1] == serial_protocol.CMD_G && m.payload[3] == serial_protocol.CMD_BANG {
        payload[0] = m.payload[2]
        return service_effect.effect_send(s.pid, service_topology.KV_ENDPOINT_ID, 1, payload)
    }

    if m.payload[0] == serial_protocol.CMD_K && m.payload[1] == serial_protocol.CMD_C && m.payload[2] == serial_protocol.CMD_BANG && m.payload[3] == serial_protocol.CMD_BANG {
        return service_effect.effect_send(s.pid, service_topology.KV_ENDPOINT_ID, 0, payload)
    }

    return invalid_effect()
}

func debug_request(s: ShellServiceState, len: usize) u32 {
    if s.pid == 0 || len == 0 {
        return 0
    }
    return 1
}
