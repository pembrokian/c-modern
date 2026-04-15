import primitives
import serial_protocol
import service_effect
import service_topology
import syscall

const SHELL_INVALID_REPLY: u8 = 63  // '?'
const SHELL_INVALID_COMMAND: u8 = 67  // 'C'
const SHELL_INVALID_SHAPE: u8 = 83  // 'S'

struct ShellServiceState {
    pid: u32
    slot: u32
}

func shell_init(pid: u32, slot: u32) ShellServiceState {
    return ShellServiceState{ pid: pid, slot: slot }
}

func invalid_effect(kind: u8) service_effect.Effect {
    payload: [4]u8 = primitives.zero_payload()
    payload[0] = SHELL_INVALID_REPLY
    payload[1] = kind
    return service_effect.effect_reply(syscall.SyscallStatus.InvalidArgument, 2, payload)
}

func handle(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    payload: [4]u8 = primitives.zero_payload()

    if s.pid == 0 || m.payload_len != 4 {
        return invalid_effect(SHELL_INVALID_SHAPE)
    }

    if m.payload[0] == serial_protocol.CMD_E {
        if m.payload[1] != serial_protocol.CMD_C {
            return invalid_effect(SHELL_INVALID_COMMAND)
        }
        payload[0] = m.payload[2]
        payload[1] = m.payload[3]
        return service_effect.effect_send(s.pid, service_topology.ECHO_ENDPOINT_ID, 2, payload)
    }

    if m.payload[0] == serial_protocol.CMD_L {
        if m.payload[1] == serial_protocol.CMD_A {
            if m.payload[3] != serial_protocol.CMD_BANG {
                return invalid_effect(SHELL_INVALID_SHAPE)
            }
            payload[0] = m.payload[2]
            return service_effect.effect_send(s.pid, service_topology.LOG_ENDPOINT_ID, 1, payload)
        }
        if m.payload[1] == serial_protocol.CMD_T {
            if m.payload[2] != serial_protocol.CMD_BANG || m.payload[3] != serial_protocol.CMD_BANG {
                return invalid_effect(SHELL_INVALID_SHAPE)
            }
            return service_effect.effect_send(s.pid, service_topology.LOG_ENDPOINT_ID, 0, payload)
        }
        return invalid_effect(SHELL_INVALID_COMMAND)
    }

    if m.payload[0] == serial_protocol.CMD_K {
        if m.payload[1] == serial_protocol.CMD_S {
            payload[0] = m.payload[2]
            payload[1] = m.payload[3]
            return service_effect.effect_send(s.pid, service_topology.KV_ENDPOINT_ID, 2, payload)
        }
        if m.payload[1] == serial_protocol.CMD_G {
            if m.payload[3] != serial_protocol.CMD_BANG {
                return invalid_effect(SHELL_INVALID_SHAPE)
            }
            payload[0] = m.payload[2]
            return service_effect.effect_send(s.pid, service_topology.KV_ENDPOINT_ID, 1, payload)
        }
        if m.payload[1] == serial_protocol.CMD_C {
            if m.payload[2] != serial_protocol.CMD_BANG || m.payload[3] != serial_protocol.CMD_BANG {
                return invalid_effect(SHELL_INVALID_SHAPE)
            }
            return service_effect.effect_send(s.pid, service_topology.KV_ENDPOINT_ID, 0, payload)
        }
        return invalid_effect(SHELL_INVALID_COMMAND)
    }

    if m.payload[0] == serial_protocol.CMD_Q {
        if m.payload[1] == serial_protocol.CMD_A {
            if m.payload[3] != serial_protocol.CMD_BANG {
                return invalid_effect(SHELL_INVALID_SHAPE)
            }
            payload[0] = m.payload[2]
            return service_effect.effect_send(s.pid, service_topology.QUEUE_ENDPOINT_ID, 1, payload)
        }
        if m.payload[1] == serial_protocol.CMD_D {
            if m.payload[2] != serial_protocol.CMD_BANG || m.payload[3] != serial_protocol.CMD_BANG {
                return invalid_effect(SHELL_INVALID_SHAPE)
            }
            return service_effect.effect_send(s.pid, service_topology.QUEUE_ENDPOINT_ID, 0, payload)
        }
        return invalid_effect(SHELL_INVALID_COMMAND)
    }

    return invalid_effect(SHELL_INVALID_COMMAND)
}

func debug_request(s: ShellServiceState, len: usize) u32 {
    if s.pid == 0 || len == 0 {
        return 0
    }
    return 1
}
