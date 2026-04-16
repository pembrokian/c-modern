import primitives
import serial_protocol
import service_effect
import service_identity
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

// lifecycle_target_endpoint maps a shell protocol target byte to the
// corresponding boot-wired endpoint id.  It lives here because this module
// is the only owner that imports both serial_protocol (target byte names)
// and service_topology (endpoint id names) without circularity.
func lifecycle_target_endpoint(target: u8) u32 {
    if target == serial_protocol.TARGET_SERIAL {
        return service_topology.SERIAL_ENDPOINT_ID
    }
    if target == serial_protocol.TARGET_SHELL {
        return service_topology.SHELL_ENDPOINT_ID
    }
    if target == serial_protocol.TARGET_LOG {
        return service_topology.LOG_ENDPOINT_ID
    }
    if target == serial_protocol.TARGET_KV {
        return service_topology.KV_ENDPOINT_ID
    }
    if target == serial_protocol.TARGET_QUEUE {
        return service_topology.QUEUE_ENDPOINT_ID
    }
    if target == serial_protocol.TARGET_ECHO {
        return service_topology.ECHO_ENDPOINT_ID
    }
    if target == serial_protocol.TARGET_TRANSFER {
        return service_topology.TRANSFER_ENDPOINT_ID
    }
    if target == serial_protocol.TARGET_TICKET {
        return service_topology.TICKET_ENDPOINT_ID
    }
    return 0
}

func lifecycle_mode(endpoint: u32) u8 {
    mode: service_topology.ServiceRestartMode = service_topology.service_restart_mode(endpoint)
    if mode == service_topology.ServiceRestartMode.Reload {
        return serial_protocol.LIFECYCLE_RELOAD
    }
    if mode == service_topology.ServiceRestartMode.Reset {
        return serial_protocol.LIFECYCLE_RESET
    }
    return serial_protocol.LIFECYCLE_NONE
}

func lifecycle_effect(status: syscall.SyscallStatus, target: u8, mode: u8) service_effect.Effect {
    payload: [4]u8 = primitives.zero_payload()
    payload[0] = target
    payload[1] = mode
    return service_effect.effect_reply(status, 2, payload)
}

func lifecycle_identity_request(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return service_effect.effect_send(s.pid, service_topology.SHELL_ENDPOINT_ID, m.payload_len, m.payload)
}

func lifecycle_identity_effect(status: syscall.SyscallStatus, generation_payload: [4]u8) service_effect.Effect {
    return service_effect.effect_reply(status, 4, generation_payload)
}

func lifecycle_restart_request(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return service_effect.effect_send(s.pid, service_topology.SHELL_ENDPOINT_ID, m.payload_len, m.payload)
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

    if m.payload[0] == serial_protocol.CMD_X {
        if m.payload[3] != serial_protocol.CMD_BANG {
            return invalid_effect(SHELL_INVALID_SHAPE)
        }
        if m.payload[1] == serial_protocol.CMD_Q {
            if m.payload[2] == serial_protocol.TARGET_WORKSET || m.payload[2] == serial_protocol.TARGET_AUDIT {
                return lifecycle_effect(syscall.SyscallStatus.Ok, m.payload[2], serial_protocol.LIFECYCLE_RELOAD)
            }
            query_endpoint: u32 = lifecycle_target_endpoint(m.payload[2])
            if query_endpoint == 0 {
                return invalid_effect(SHELL_INVALID_COMMAND)
            }
            return lifecycle_effect(syscall.SyscallStatus.Ok, m.payload[2], lifecycle_mode(query_endpoint))
        }
        if m.payload[1] == serial_protocol.CMD_I {
            if m.payload[2] != serial_protocol.TARGET_WORKSET {
                identity_endpoint: u32 = lifecycle_target_endpoint(m.payload[2])
                if identity_endpoint == 0 {
                    return invalid_effect(SHELL_INVALID_COMMAND)
                }
            }
            return lifecycle_identity_request(s, m)
        }
        if m.payload[1] == serial_protocol.CMD_R {
            if m.payload[2] == serial_protocol.TARGET_WORKSET || m.payload[2] == serial_protocol.TARGET_AUDIT {
                return lifecycle_restart_request(s, m)
            }
            restart_endpoint: u32 = lifecycle_target_endpoint(m.payload[2])
            if restart_endpoint == 0 {
                return invalid_effect(SHELL_INVALID_COMMAND)
            }
            return lifecycle_restart_request(s, m)
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
        if m.payload[1] == serial_protocol.CMD_C || m.payload[1] == serial_protocol.CMD_P {
            if m.payload[2] != serial_protocol.CMD_BANG || m.payload[3] != serial_protocol.CMD_BANG {
                return invalid_effect(SHELL_INVALID_SHAPE)
            }
            payload[0] = m.payload[1]
            payload[1] = serial_protocol.CMD_BANG
            return service_effect.effect_send(s.pid, service_topology.QUEUE_ENDPOINT_ID, 2, payload)
        }
        return invalid_effect(SHELL_INVALID_COMMAND)
    }

    if m.payload[0] == serial_protocol.CMD_T {
        if m.payload[1] == serial_protocol.CMD_I {
            if m.payload[2] != serial_protocol.CMD_BANG || m.payload[3] != serial_protocol.CMD_BANG {
                return invalid_effect(SHELL_INVALID_SHAPE)
            }
            return service_effect.effect_send(s.pid, service_topology.TICKET_ENDPOINT_ID, 0, payload)
        }
        if m.payload[1] == serial_protocol.CMD_U {
            payload[0] = m.payload[2]
            payload[1] = m.payload[3]
            return service_effect.effect_send(s.pid, service_topology.TICKET_ENDPOINT_ID, 2, payload)
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
