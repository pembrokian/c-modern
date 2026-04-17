import primitives
import identity_taxonomy
import serial_protocol
import service_effect
import service_topology
import service_state
import syscall

const SHELL_INVALID_REPLY: u8 = 63  // '?'
const SHELL_INVALID_COMMAND: u8 = 67  // 'C'
const SHELL_INVALID_SHAPE: u8 = 83  // 'S'

struct LifecycleControl {
    op: u8
    target: u8
}

struct ShellServiceState {
    pid: u32
    slot: u32
}

struct FamilyRoute {
    endpoint: u32
    send_len: usize
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

func forward_payload(s: ShellServiceState, endpoint: u32, send_len: usize, payload: [4]u8) service_effect.Effect {
    return service_effect.effect_send(s.pid, endpoint, send_len, payload)
}

func forward_empty(s: ShellServiceState, endpoint: u32) service_effect.Effect {
    return forward_payload(s, endpoint, 0, primitives.zero_payload())
}

func forward_one(s: ShellServiceState, endpoint: u32, value0: u8) service_effect.Effect {
    payload: [4]u8 = primitives.zero_payload()
    payload[0] = value0
    return forward_payload(s, endpoint, 1, payload)
}

func forward_two(s: ShellServiceState, endpoint: u32, value0: u8, value1: u8) service_effect.Effect {
    payload: [4]u8 = primitives.zero_payload()
    payload[0] = value0
    payload[1] = value1
    return forward_payload(s, endpoint, 2, payload)
}

func forward_three(s: ShellServiceState, endpoint: u32, value0: u8, value1: u8, value2: u8) service_effect.Effect {
    payload: [4]u8 = primitives.zero_payload()
    payload[0] = value0
    payload[1] = value1
    payload[2] = value2
    return forward_payload(s, endpoint, 3, payload)
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

func lifecycle_op_supported(op: u8) bool {
    return op == serial_protocol.CMD_A || op == serial_protocol.CMD_C || op == serial_protocol.CMD_D || op == serial_protocol.CMD_I || op == serial_protocol.CMD_M || op == serial_protocol.CMD_P || op == serial_protocol.CMD_S || op == serial_protocol.CMD_R
}

func lifecycle_control(m: service_effect.Message) LifecycleControl {
    return LifecycleControl{ op: m.payload[1], target: m.payload[2] }
}

func lifecycle_invalid_kind(m: service_effect.Message) u8 {
    if m.payload_len != 4 {
        return SHELL_INVALID_SHAPE
    }
    if m.payload[0] != serial_protocol.CMD_X {
        return SHELL_INVALID_COMMAND
    }
    if !lifecycle_op_supported(m.payload[1]) {
        return SHELL_INVALID_COMMAND
    }
    if m.payload[3] != serial_protocol.CMD_BANG {
        return SHELL_INVALID_SHAPE
    }
    return 0
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
    case serial_protocol.TARGET_FILE:
        return service_topology.FILE_ENDPOINT_ID
    case serial_protocol.TARGET_TIMER:
        return service_topology.TIMER_ENDPOINT_ID
    case serial_protocol.TARGET_TASK:
        return service_topology.TASK_ENDPOINT_ID
    case serial_protocol.TARGET_JOURNAL:
        return service_topology.JOURNAL_ENDPOINT_ID
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

func lifecycle_authority_request(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return service_effect.effect_send(s.pid, service_topology.SHELL_ENDPOINT_ID, m.payload_len, m.payload)
}

func lifecycle_authority_effect(status: syscall.SyscallStatus, payload: [4]u8) service_effect.Effect {
    return service_effect.effect_reply(status, 4, payload)
}

func lifecycle_state_request(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return service_effect.effect_send(s.pid, service_topology.SHELL_ENDPOINT_ID, m.payload_len, m.payload)
}

func lifecycle_state_effect(status: syscall.SyscallStatus, payload: [4]u8) service_effect.Effect {
    return service_effect.effect_reply(status, 4, payload)
}

func lifecycle_durability_request(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return service_effect.effect_send(s.pid, service_topology.SHELL_ENDPOINT_ID, m.payload_len, m.payload)
}

func lifecycle_durability_effect(status: syscall.SyscallStatus, payload: [4]u8) service_effect.Effect {
    return service_effect.effect_reply(status, 4, payload)
}

func lifecycle_policy_effect(status: syscall.SyscallStatus, payload: [4]u8) service_effect.Effect {
    return service_effect.effect_reply(status, 4, payload)
}

func lifecycle_summary_request(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return service_effect.effect_send(s.pid, service_topology.SHELL_ENDPOINT_ID, m.payload_len, m.payload)
}

func lifecycle_compare_request(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return service_effect.effect_send(s.pid, service_topology.SHELL_ENDPOINT_ID, m.payload_len, m.payload)
}

func lifecycle_summary_effect(status: syscall.SyscallStatus, payload: [4]u8) service_effect.Effect {
    return service_effect.effect_reply(status, 4, payload)
}

func lifecycle_request(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return service_effect.effect_send(s.pid, service_topology.SHELL_ENDPOINT_ID, m.payload_len, m.payload)
}

func route_log(s: ShellServiceState, m: service_effect.Message, op: u8) service_effect.Effect {
    switch op {
    case serial_protocol.CMD_A:
        if !bang3(m) {
            return invalid_effect(SHELL_INVALID_SHAPE)
        }
        return forward_one(s, service_topology.LOG_ENDPOINT_ID, m.payload[2])
    case serial_protocol.CMD_T:
        if !bang23(m) {
            return invalid_effect(SHELL_INVALID_SHAPE)
        }
        return forward_empty(s, service_topology.LOG_ENDPOINT_ID)
    default:
        return invalid_effect(SHELL_INVALID_COMMAND)
    }
}

func route_kv(s: ShellServiceState, m: service_effect.Message, op: u8) service_effect.Effect {
    switch op {
    case serial_protocol.CMD_S:
        return forward_two(s, service_topology.KV_ENDPOINT_ID, m.payload[2], m.payload[3])
    case serial_protocol.CMD_G:
        if !bang3(m) {
            return invalid_effect(SHELL_INVALID_SHAPE)
        }
        return forward_one(s, service_topology.KV_ENDPOINT_ID, m.payload[2])
    case serial_protocol.CMD_C:
        if !bang23(m) {
            return invalid_effect(SHELL_INVALID_SHAPE)
        }
        return forward_empty(s, service_topology.KV_ENDPOINT_ID)
    default:
        return invalid_effect(SHELL_INVALID_COMMAND)
    }
}

func queue_route(op: u8) FamilyRoute {
    switch op {
    case serial_protocol.CMD_A:
        return FamilyRoute{ endpoint: service_topology.QUEUE_ENDPOINT_ID, send_len: 1 }
    case serial_protocol.CMD_D:
        return FamilyRoute{ endpoint: service_topology.QUEUE_ENDPOINT_ID, send_len: 0 }
    case serial_protocol.CMD_C:
        return FamilyRoute{ endpoint: service_topology.QUEUE_ENDPOINT_ID, send_len: 2 }
    case serial_protocol.CMD_P:
        return FamilyRoute{ endpoint: service_topology.QUEUE_ENDPOINT_ID, send_len: 2 }
    case serial_protocol.CMD_W:
        return FamilyRoute{ endpoint: service_topology.QUEUE_ENDPOINT_ID, send_len: 2 }
    default:
        return FamilyRoute{ endpoint: 0, send_len: 0 }
    }
}

func route_queue(s: ShellServiceState, m: service_effect.Message, op: u8) service_effect.Effect {
    route := queue_route(op)
    if route.endpoint == 0 {
        return invalid_effect(SHELL_INVALID_COMMAND)
    }
    if op == serial_protocol.CMD_A {
        if !bang3(m) {
            return invalid_effect(SHELL_INVALID_SHAPE)
        }
        return forward_one(s, route.endpoint, m.payload[2])
    }
    if op == serial_protocol.CMD_D {
        if !bang23(m) {
            return invalid_effect(SHELL_INVALID_SHAPE)
        }
        return forward_empty(s, route.endpoint)
    }
    if !bang23(m) {
        return invalid_effect(SHELL_INVALID_SHAPE)
    }
    return forward_two(s, route.endpoint, op, serial_protocol.CMD_BANG)
}

func file_route(op: u8) FamilyRoute {
    switch op {
    case serial_protocol.CMD_C:
        return FamilyRoute{ endpoint: service_topology.FILE_ENDPOINT_ID, send_len: 2 }
    case serial_protocol.CMD_W:
        return FamilyRoute{ endpoint: service_topology.FILE_ENDPOINT_ID, send_len: 3 }
    case serial_protocol.CMD_R:
        return FamilyRoute{ endpoint: service_topology.FILE_ENDPOINT_ID, send_len: 2 }
    case serial_protocol.CMD_L:
        return FamilyRoute{ endpoint: service_topology.FILE_ENDPOINT_ID, send_len: 0 }
    default:
        return FamilyRoute{ endpoint: 0, send_len: 0 }
    }
}

func route_file(s: ShellServiceState, m: service_effect.Message, op: u8) service_effect.Effect {
    route := file_route(op)
    if route.endpoint == 0 {
        return invalid_effect(SHELL_INVALID_COMMAND)
    }
    switch op {
    case serial_protocol.CMD_C:
        if !bang3(m) {
            return invalid_effect(SHELL_INVALID_SHAPE)
        }
        return forward_two(s, route.endpoint, serial_protocol.CMD_C, m.payload[2])
    case serial_protocol.CMD_W:
        return forward_three(s, route.endpoint, serial_protocol.CMD_W, m.payload[2], m.payload[3])
    case serial_protocol.CMD_R:
        if !bang3(m) {
            return invalid_effect(SHELL_INVALID_SHAPE)
        }
        return forward_two(s, route.endpoint, serial_protocol.CMD_R, m.payload[2])
    case serial_protocol.CMD_L:
        if !bang23(m) {
            return invalid_effect(SHELL_INVALID_SHAPE)
        }
        return forward_empty(s, route.endpoint)
    default:
        return invalid_effect(SHELL_INVALID_COMMAND)
    }
}

func timer_route(op: u8) FamilyRoute {
    switch op {
    case serial_protocol.CMD_C:
        return FamilyRoute{ endpoint: service_topology.TIMER_ENDPOINT_ID, send_len: 3 }
    case serial_protocol.CMD_X:
        return FamilyRoute{ endpoint: service_topology.TIMER_ENDPOINT_ID, send_len: 2 }
    case serial_protocol.CMD_Q:
        return FamilyRoute{ endpoint: service_topology.TIMER_ENDPOINT_ID, send_len: 2 }
    case serial_protocol.CMD_E:
        return FamilyRoute{ endpoint: service_topology.TIMER_ENDPOINT_ID, send_len: 2 }
    default:
        return FamilyRoute{ endpoint: 0, send_len: 0 }
    }
}

func route_timer(s: ShellServiceState, m: service_effect.Message, op: u8) service_effect.Effect {
    route := timer_route(op)
    if route.endpoint == 0 {
        return invalid_effect(SHELL_INVALID_COMMAND)
    }
    if op == serial_protocol.CMD_C {
        return forward_three(s, route.endpoint, serial_protocol.CMD_C, m.payload[2], m.payload[3])
    }
    if !bang3(m) {
        return invalid_effect(SHELL_INVALID_SHAPE)
    }
    return forward_two(s, route.endpoint, op, m.payload[2])
}

func journal_route(op: u8) FamilyRoute {
    switch op {
    case serial_protocol.CMD_A:
        return FamilyRoute{ endpoint: service_topology.JOURNAL_ENDPOINT_ID, send_len: 3 }
    case serial_protocol.CMD_R:
        return FamilyRoute{ endpoint: service_topology.JOURNAL_ENDPOINT_ID, send_len: 2 }
    case serial_protocol.CMD_C:
        return FamilyRoute{ endpoint: service_topology.JOURNAL_ENDPOINT_ID, send_len: 2 }
    default:
        return FamilyRoute{ endpoint: 0, send_len: 0 }
    }
}

func route_journal(s: ShellServiceState, m: service_effect.Message, op: u8) service_effect.Effect {
    route := journal_route(op)
    if route.endpoint == 0 {
        return invalid_effect(SHELL_INVALID_COMMAND)
    }
    if op == serial_protocol.CMD_A {
        return forward_three(s, route.endpoint, serial_protocol.CMD_A, m.payload[2], m.payload[3])
    }
    if !bang3(m) {
        return invalid_effect(SHELL_INVALID_SHAPE)
    }
    return forward_two(s, route.endpoint, op, m.payload[2])
}

func task_route(op: u8) FamilyRoute {
    switch op {
    case serial_protocol.CMD_S:
        return FamilyRoute{ endpoint: service_topology.TASK_ENDPOINT_ID, send_len: 2 }
    case serial_protocol.CMD_Q:
        return FamilyRoute{ endpoint: service_topology.TASK_ENDPOINT_ID, send_len: 2 }
    case serial_protocol.CMD_C:
        return FamilyRoute{ endpoint: service_topology.TASK_ENDPOINT_ID, send_len: 2 }
    case serial_protocol.CMD_D:
        return FamilyRoute{ endpoint: service_topology.TASK_ENDPOINT_ID, send_len: 2 }
    case serial_protocol.CMD_L:
        return FamilyRoute{ endpoint: service_topology.TASK_ENDPOINT_ID, send_len: 2 }
    default:
        return FamilyRoute{ endpoint: 0, send_len: 0 }
    }
}

func route_task(s: ShellServiceState, m: service_effect.Message, op: u8) service_effect.Effect {
    route := task_route(op)
    if route.endpoint == 0 {
        return invalid_effect(SHELL_INVALID_COMMAND)
    }
    if !bang3(m) {
        return invalid_effect(SHELL_INVALID_SHAPE)
    }
    return forward_two(s, route.endpoint, op, m.payload[2])
}

func route_ticket(s: ShellServiceState, m: service_effect.Message, op: u8) service_effect.Effect {
    switch op {
    case serial_protocol.CMD_I:
        if !bang23(m) {
            return invalid_effect(SHELL_INVALID_SHAPE)
        }
        return forward_empty(s, service_topology.TICKET_ENDPOINT_ID)
    case serial_protocol.CMD_U:
        return forward_two(s, service_topology.TICKET_ENDPOINT_ID, m.payload[2], m.payload[3])
    default:
        return invalid_effect(SHELL_INVALID_COMMAND)
    }
}

func handle(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
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
        return forward_two(s, service_topology.ECHO_ENDPOINT_ID, m.payload[2], m.payload[3])

    case serial_protocol.CMD_L:
        return route_log(s, m, op)

    case serial_protocol.CMD_K:
        return route_kv(s, m, op)

    case serial_protocol.CMD_X:
        if !bang3(m) {
            return invalid_effect(SHELL_INVALID_SHAPE)
        }
        switch op {
        case serial_protocol.CMD_A:
            if !identity_taxonomy.identity_target_is_lane(m.payload[2]) {
                authority_endpoint: u32 = lifecycle_target_endpoint(m.payload[2])
                if authority_endpoint == 0 {
                    return invalid_effect(SHELL_INVALID_COMMAND)
                }
            }
            return lifecycle_authority_request(s, m)
        case serial_protocol.CMD_C:
            if !identity_taxonomy.identity_target_is_lane(m.payload[2]) {
                state_endpoint: u32 = lifecycle_target_endpoint(m.payload[2])
                if state_endpoint == 0 {
                    return invalid_effect(SHELL_INVALID_COMMAND)
                }
            }
            return lifecycle_state_request(s, m)
        case serial_protocol.CMD_D:
            if !identity_taxonomy.identity_target_is_lane(m.payload[2]) {
                durability_endpoint: u32 = lifecycle_target_endpoint(m.payload[2])
                if durability_endpoint == 0 {
                    return invalid_effect(SHELL_INVALID_COMMAND)
                }
            }
            return lifecycle_durability_request(s, m)
        case serial_protocol.CMD_Q:
            if identity_taxonomy.identity_target_is_lane(m.payload[2]) {
                return lifecycle_effect(syscall.SyscallStatus.Ok, m.payload[2], serial_protocol.LIFECYCLE_RELOAD)
            }
            query_endpoint: u32 = lifecycle_target_endpoint(m.payload[2])
            if query_endpoint == 0 {
                return invalid_effect(SHELL_INVALID_COMMAND)
            }
            return lifecycle_effect(syscall.SyscallStatus.Ok, m.payload[2], lifecycle_mode(query_endpoint))
        case serial_protocol.CMD_I:
            if !identity_taxonomy.identity_target_has_generation_query(m.payload[2]) {
                return invalid_effect(SHELL_INVALID_COMMAND)
            }
            if !identity_taxonomy.identity_target_has_direct_generation(m.payload[2]) {
                return lifecycle_identity_request(s, m)
            }
            if m.payload[2] != serial_protocol.TARGET_WORKSET {
                identity_endpoint: u32 = lifecycle_target_endpoint(m.payload[2])
                if identity_endpoint == 0 {
                    return invalid_effect(SHELL_INVALID_COMMAND)
                }
            }
            return lifecycle_identity_request(s, m)
        case serial_protocol.CMD_P:
            if !identity_taxonomy.identity_target_is_lane(m.payload[2]) {
                policy_endpoint: u32 = lifecycle_target_endpoint(m.payload[2])
                if policy_endpoint == 0 || !service_topology.service_can_restart(policy_endpoint) {
                    return invalid_effect(SHELL_INVALID_COMMAND)
                }
            }
            return lifecycle_policy_request(s, m)
        case serial_protocol.CMD_S:
            if !identity_taxonomy.identity_target_is_lane(m.payload[2]) {
                summary_endpoint: u32 = lifecycle_target_endpoint(m.payload[2])
                if summary_endpoint == 0 || !service_topology.service_can_restart(summary_endpoint) {
                    return invalid_effect(SHELL_INVALID_COMMAND)
                }
            }
            return lifecycle_summary_request(s, m)
        case serial_protocol.CMD_M:
            if !identity_taxonomy.identity_target_is_lane(m.payload[2]) {
                compare_endpoint: u32 = lifecycle_target_endpoint(m.payload[2])
                if compare_endpoint == 0 || !service_topology.service_can_restart(compare_endpoint) {
                    return invalid_effect(SHELL_INVALID_COMMAND)
                }
            }
            return lifecycle_compare_request(s, m)
        case serial_protocol.CMD_R:
            if identity_taxonomy.identity_target_is_lane(m.payload[2]) {
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
        return route_queue(s, m, op)

    case serial_protocol.CMD_F:
        return route_file(s, m, op)

    case serial_protocol.CMD_M:
        return route_timer(s, m, op)

    case serial_protocol.CMD_Y:
        return route_journal(s, m, op)

    case serial_protocol.CMD_J:
        return route_task(s, m, op)

    case serial_protocol.CMD_T:
        return route_ticket(s, m, op)
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
