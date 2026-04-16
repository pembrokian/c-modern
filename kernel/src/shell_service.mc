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

func shell_ready(s: ShellServiceState) bool {
    return s.pid != 0
}

func bang3(m: service_effect.Message) bool {
    return m.payload[3] == serial_protocol.CMD_BANG
}

func bang23(m: service_effect.Message) bool {
    return m.payload[2] == serial_protocol.CMD_BANG && m.payload[3] == serial_protocol.CMD_BANG
}

// lifecycle_target_endpoint maps a shell protocol target byte to the
// corresponding boot-wired endpoint id.  It lives here because this module
// is the only owner that imports both serial_protocol (target byte names)
// and service_topology (endpoint id names) without circularity.
func lifecycle_target_endpoint(target: u8) u32 {
    switch target {
    case serial_protocol.TARGET_SERIAL:
        return service_topology.SERIAL_ENDPOINT_ID
    case serial_protocol.TARGET_SHELL:
        return service_topology.SHELL_ENDPOINT_ID
    case serial_protocol.TARGET_LOG:
        return service_topology.LOG_ENDPOINT_ID
    case serial_protocol.TARGET_KV:
        return service_topology.KV_ENDPOINT_ID
    case serial_protocol.TARGET_QUEUE:
        return service_topology.QUEUE_ENDPOINT_ID
    case serial_protocol.TARGET_ECHO:
        return service_topology.ECHO_ENDPOINT_ID
    case serial_protocol.TARGET_TRANSFER:
        return service_topology.TRANSFER_ENDPOINT_ID
    case serial_protocol.TARGET_TICKET:
        return service_topology.TICKET_ENDPOINT_ID
    default:
        return 0
    }
}

func lifecycle_mode(endpoint: u32) u8 {
    mode: service_topology.ServiceRestartMode = service_topology.service_restart_mode(endpoint)
    switch mode {
    case service_topology.ServiceRestartMode.Reload:
        return serial_protocol.LIFECYCLE_RELOAD
    case service_topology.ServiceRestartMode.Reset:
        return serial_protocol.LIFECYCLE_RESET
    default:
        return serial_protocol.LIFECYCLE_NONE
    }
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

func lifecycle_policy_request(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return service_effect.effect_send(s.pid, service_topology.SHELL_ENDPOINT_ID, m.payload_len, m.payload)
}

func lifecycle_policy_effect(status: syscall.SyscallStatus, payload: [4]u8) service_effect.Effect {
    return service_effect.effect_reply(status, 4, payload)
}

func lifecycle_summary_request(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return service_effect.effect_send(s.pid, service_topology.SHELL_ENDPOINT_ID, m.payload_len, m.payload)
}

func lifecycle_summary_effect(status: syscall.SyscallStatus, payload: [4]u8) service_effect.Effect {
    return service_effect.effect_reply(status, 4, payload)
}

func lifecycle_request(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return service_effect.effect_send(s.pid, service_topology.SHELL_ENDPOINT_ID, m.payload_len, m.payload)
}

func handle(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    payload: [4]u8 = primitives.zero_payload()
    cmd: u8 = m.payload[0]
    op: u8 = m.payload[1]

    if !shell_ready(s) || m.payload_len != 4 {
        return invalid_effect(SHELL_INVALID_SHAPE)
    }

    switch cmd {
    case serial_protocol.CMD_E:
        if op != serial_protocol.CMD_C {
            return invalid_effect(SHELL_INVALID_COMMAND)
        }
        payload[0] = m.payload[2]
        payload[1] = m.payload[3]
        return service_effect.effect_send(s.pid, service_topology.ECHO_ENDPOINT_ID, 2, payload)

    case serial_protocol.CMD_L:
        switch op {
        case serial_protocol.CMD_A:
            if !bang3(m) {
                return invalid_effect(SHELL_INVALID_SHAPE)
            }
            payload[0] = m.payload[2]
            return service_effect.effect_send(s.pid, service_topology.LOG_ENDPOINT_ID, 1, payload)
        case serial_protocol.CMD_T:
            if !bang23(m) {
                return invalid_effect(SHELL_INVALID_SHAPE)
            }
            return service_effect.effect_send(s.pid, service_topology.LOG_ENDPOINT_ID, 0, payload)
        default:
            return invalid_effect(SHELL_INVALID_COMMAND)
        }

    case serial_protocol.CMD_K:
        switch op {
        case serial_protocol.CMD_S:
            payload[0] = m.payload[2]
            payload[1] = m.payload[3]
            return service_effect.effect_send(s.pid, service_topology.KV_ENDPOINT_ID, 2, payload)
        case serial_protocol.CMD_G:
            if !bang3(m) {
                return invalid_effect(SHELL_INVALID_SHAPE)
            }
            payload[0] = m.payload[2]
            return service_effect.effect_send(s.pid, service_topology.KV_ENDPOINT_ID, 1, payload)
        case serial_protocol.CMD_C:
            if !bang23(m) {
                return invalid_effect(SHELL_INVALID_SHAPE)
            }
            return service_effect.effect_send(s.pid, service_topology.KV_ENDPOINT_ID, 0, payload)
        default:
            return invalid_effect(SHELL_INVALID_COMMAND)
        }

    case serial_protocol.CMD_X:
        if !bang3(m) {
            return invalid_effect(SHELL_INVALID_SHAPE)
        }
        switch op {
        case serial_protocol.CMD_Q:
            if m.payload[2] == serial_protocol.TARGET_WORKSET || m.payload[2] == serial_protocol.TARGET_AUDIT {
                return lifecycle_effect(syscall.SyscallStatus.Ok, m.payload[2], serial_protocol.LIFECYCLE_RELOAD)
            }
            query_endpoint: u32 = lifecycle_target_endpoint(m.payload[2])
            if query_endpoint == 0 {
                return invalid_effect(SHELL_INVALID_COMMAND)
            }
            return lifecycle_effect(syscall.SyscallStatus.Ok, m.payload[2], lifecycle_mode(query_endpoint))
        case serial_protocol.CMD_I:
            if m.payload[2] != serial_protocol.TARGET_WORKSET {
                identity_endpoint: u32 = lifecycle_target_endpoint(m.payload[2])
                if identity_endpoint == 0 {
                    return invalid_effect(SHELL_INVALID_COMMAND)
                }
            }
            return lifecycle_identity_request(s, m)
        case serial_protocol.CMD_P:
            if m.payload[2] != serial_protocol.TARGET_WORKSET && m.payload[2] != serial_protocol.TARGET_AUDIT {
                policy_endpoint: u32 = lifecycle_target_endpoint(m.payload[2])
                if policy_endpoint == 0 || !service_topology.service_can_restart(policy_endpoint) {
                    return invalid_effect(SHELL_INVALID_COMMAND)
                }
            }
            return lifecycle_policy_request(s, m)
        case serial_protocol.CMD_S:
            if m.payload[2] != serial_protocol.TARGET_WORKSET && m.payload[2] != serial_protocol.TARGET_AUDIT {
                summary_endpoint: u32 = lifecycle_target_endpoint(m.payload[2])
                if summary_endpoint == 0 || !service_topology.service_can_restart(summary_endpoint) {
                    return invalid_effect(SHELL_INVALID_COMMAND)
                }
            }
            return lifecycle_summary_request(s, m)
        case serial_protocol.CMD_R:
            if m.payload[2] == serial_protocol.TARGET_WORKSET || m.payload[2] == serial_protocol.TARGET_AUDIT {
                return lifecycle_request(s, m)
            }
            restart_endpoint: u32 = lifecycle_target_endpoint(m.payload[2])
            if restart_endpoint == 0 {
                return invalid_effect(SHELL_INVALID_COMMAND)
            }
            return lifecycle_request(s, m)
        default:
            return invalid_effect(SHELL_INVALID_COMMAND)
        }

    case serial_protocol.CMD_Q:
        switch op {
        case serial_protocol.CMD_A:
            if !bang3(m) {
                return invalid_effect(SHELL_INVALID_SHAPE)
            }
            payload[0] = m.payload[2]
            return service_effect.effect_send(s.pid, service_topology.QUEUE_ENDPOINT_ID, 1, payload)
        case serial_protocol.CMD_D:
            if !bang23(m) {
                return invalid_effect(SHELL_INVALID_SHAPE)
            }
            return service_effect.effect_send(s.pid, service_topology.QUEUE_ENDPOINT_ID, 0, payload)
        case serial_protocol.CMD_C:
            if !bang23(m) {
                return invalid_effect(SHELL_INVALID_SHAPE)
            }
            payload[0] = op
            payload[1] = serial_protocol.CMD_BANG
            return service_effect.effect_send(s.pid, service_topology.QUEUE_ENDPOINT_ID, 2, payload)
        case serial_protocol.CMD_P:
            if !bang23(m) {
                return invalid_effect(SHELL_INVALID_SHAPE)
            }
            payload[0] = op
            payload[1] = serial_protocol.CMD_BANG
            return service_effect.effect_send(s.pid, service_topology.QUEUE_ENDPOINT_ID, 2, payload)
        default:
            return invalid_effect(SHELL_INVALID_COMMAND)
        }

    case serial_protocol.CMD_T:
        switch op {
        case serial_protocol.CMD_I:
            if !bang23(m) {
                return invalid_effect(SHELL_INVALID_SHAPE)
            }
            return service_effect.effect_send(s.pid, service_topology.TICKET_ENDPOINT_ID, 0, payload)
        case serial_protocol.CMD_U:
            payload[0] = m.payload[2]
            payload[1] = m.payload[3]
            return service_effect.effect_send(s.pid, service_topology.TICKET_ENDPOINT_ID, 2, payload)
        default:
            return invalid_effect(SHELL_INVALID_COMMAND)
        }
    default:
        return invalid_effect(SHELL_INVALID_COMMAND)
    }

    return invalid_effect(SHELL_INVALID_COMMAND)
}

func debug_request(s: ShellServiceState, len: usize) u32 {
    if !shell_ready(s) || len == 0 {
        return 0
    }
    return 1
}
