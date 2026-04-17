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

struct OpHandler {
    op: u8
    check: func(service_effect.Message) bool
    run: func(ShellServiceState, service_effect.Message) service_effect.Effect
}

const ROUTE_SENTINEL_OP: u8 = 0

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

func anymsg(m: service_effect.Message) bool {
    return true
}

func invalid_route(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return invalid_effect(SHELL_INVALID_COMMAND)
}

const UNUSED_ROUTE: OpHandler = OpHandler{
    op: ROUTE_SENTINEL_OP,
    check: anymsg,
    run: invalid_route
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
    case serial_protocol.TARGET_WORKFLOW:
        return service_topology.WORKFLOW_ENDPOINT_ID
    case serial_protocol.TARGET_LEASE:
        return service_topology.LEASE_ENDPOINT_ID
    case serial_protocol.TARGET_COMPLETION:
        return service_topology.COMPLETION_MAILBOX_ENDPOINT_ID
    case serial_protocol.TARGET_OBJECT_STORE:
        return service_topology.OBJECT_STORE_ENDPOINT_ID
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

func lifecycle_forward(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return service_effect.effect_send(s.pid, service_topology.SHELL_ENDPOINT_ID, m.payload_len, m.payload)
}

func lifecycle_identity_effect(status: syscall.SyscallStatus, generation_payload: [4]u8) service_effect.Effect {
    return service_effect.effect_reply(status, 4, generation_payload)
}

func lifecycle_authority_effect(status: syscall.SyscallStatus, payload: [4]u8) service_effect.Effect {
    return service_effect.effect_reply(status, 4, payload)
}

func lifecycle_state_effect(status: syscall.SyscallStatus, payload: [4]u8) service_effect.Effect {
    return service_effect.effect_reply(status, 4, payload)
}

func lifecycle_durability_effect(status: syscall.SyscallStatus, payload: [4]u8) service_effect.Effect {
    return service_effect.effect_reply(status, 4, payload)
}

func lifecycle_policy_effect(status: syscall.SyscallStatus, payload: [4]u8) service_effect.Effect {
    return service_effect.effect_reply(status, 4, payload)
}

func lifecycle_summary_effect(status: syscall.SyscallStatus, payload: [4]u8) service_effect.Effect {
    return service_effect.effect_reply(status, 4, payload)
}

func dispatch(routes: [5]OpHandler, s: ShellServiceState, m: service_effect.Message, op: u8) service_effect.Effect {
    for i in 0..5 {
        if routes[i].op == ROUTE_SENTINEL_OP {
            return invalid_effect(SHELL_INVALID_COMMAND)
        }
        if routes[i].op == op {
            if !routes[i].check(m) {
                return invalid_effect(SHELL_INVALID_SHAPE)
            }
            return routes[i].run(s, m)
        }
    }
    return invalid_effect(SHELL_INVALID_COMMAND)
}

func lifecycle_validate_target(target: u8, require_restart: bool) bool {
    if identity_taxonomy.identity_target_is_lane(target) {
        return true
    }

    endpoint: u32 = lifecycle_target_endpoint(target)
    if endpoint == 0 {
        return false
    }

    if require_restart && !service_topology.service_can_restart(endpoint) {
        return false
    }

    return true
}

func route_lifecycle(s: ShellServiceState, m: service_effect.Message, op: u8) service_effect.Effect {
    if !bang3(m) {
        return invalid_effect(SHELL_INVALID_SHAPE)
    }

    switch op {
    case serial_protocol.CMD_A:
        if !lifecycle_validate_target(m.payload[2], false) {
            return invalid_effect(SHELL_INVALID_COMMAND)
        }
        return lifecycle_forward(s, m)
    case serial_protocol.CMD_C:
        if !lifecycle_validate_target(m.payload[2], false) {
            return invalid_effect(SHELL_INVALID_COMMAND)
        }
        return lifecycle_forward(s, m)
    case serial_protocol.CMD_D:
        if !lifecycle_validate_target(m.payload[2], false) {
            return invalid_effect(SHELL_INVALID_COMMAND)
        }
        return lifecycle_forward(s, m)
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
            return lifecycle_forward(s, m)
        }
        if m.payload[2] != serial_protocol.TARGET_WORKSET {
            identity_endpoint: u32 = lifecycle_target_endpoint(m.payload[2])
            if identity_endpoint == 0 {
                return invalid_effect(SHELL_INVALID_COMMAND)
            }
        }
        return lifecycle_forward(s, m)
    case serial_protocol.CMD_P:
        if !lifecycle_validate_target(m.payload[2], true) {
            return invalid_effect(SHELL_INVALID_COMMAND)
        }
        return lifecycle_forward(s, m)
    case serial_protocol.CMD_S:
        if !lifecycle_validate_target(m.payload[2], true) {
            return invalid_effect(SHELL_INVALID_COMMAND)
        }
        return lifecycle_forward(s, m)
    case serial_protocol.CMD_M:
        if !lifecycle_validate_target(m.payload[2], true) {
            return invalid_effect(SHELL_INVALID_COMMAND)
        }
        return lifecycle_forward(s, m)
    case serial_protocol.CMD_R:
        if !lifecycle_validate_target(m.payload[2], false) {
            return invalid_effect(SHELL_INVALID_COMMAND)
        }
        return lifecycle_forward(s, m)
    default:
        return invalid_effect(SHELL_INVALID_COMMAND)
    }
}

func route_echo(s: ShellServiceState, m: service_effect.Message, op: u8) service_effect.Effect {
    if op != serial_protocol.CMD_C {
        return invalid_effect(SHELL_INVALID_COMMAND)
    }
    return forward_two(s, service_topology.ECHO_ENDPOINT_ID, m.payload[2], m.payload[3])
}

func log_append(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward_one(s, service_topology.LOG_ENDPOINT_ID, m.payload[2])
}

func log_tail(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward_empty(s, service_topology.LOG_ENDPOINT_ID)
}

const LOG_ROUTES: [5]OpHandler = [5]OpHandler{
    OpHandler{ op: serial_protocol.CMD_A, check: bang3, run: log_append },
    OpHandler{ op: serial_protocol.CMD_T, check: bang23, run: log_tail },
    UNUSED_ROUTE,
    UNUSED_ROUTE,
    UNUSED_ROUTE
}

func route_log(s: ShellServiceState, m: service_effect.Message, op: u8) service_effect.Effect {
    return dispatch(LOG_ROUTES, s, m, op)
}

func kv_set(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward_two(s, service_topology.KV_ENDPOINT_ID, m.payload[2], m.payload[3])
}

func kv_get(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward_one(s, service_topology.KV_ENDPOINT_ID, m.payload[2])
}

func kv_count(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward_empty(s, service_topology.KV_ENDPOINT_ID)
}

const KV_ROUTES: [5]OpHandler = [5]OpHandler{
    OpHandler{ op: serial_protocol.CMD_S, check: anymsg, run: kv_set },
    OpHandler{ op: serial_protocol.CMD_G, check: bang3, run: kv_get },
    OpHandler{ op: serial_protocol.CMD_C, check: bang23, run: kv_count },
    UNUSED_ROUTE,
    UNUSED_ROUTE
}

func route_kv(s: ShellServiceState, m: service_effect.Message, op: u8) service_effect.Effect {
    return dispatch(KV_ROUTES, s, m, op)
}

func queue_push(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward_one(s, service_topology.QUEUE_ENDPOINT_ID, m.payload[2])
}

func queue_dequeue(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward_empty(s, service_topology.QUEUE_ENDPOINT_ID)
}

func queue_count(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward_two(s, service_topology.QUEUE_ENDPOINT_ID, serial_protocol.CMD_C, serial_protocol.CMD_BANG)
}

func queue_peek(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward_two(s, service_topology.QUEUE_ENDPOINT_ID, serial_protocol.CMD_P, serial_protocol.CMD_BANG)
}

func queue_wait(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward_two(s, service_topology.QUEUE_ENDPOINT_ID, serial_protocol.CMD_W, serial_protocol.CMD_BANG)
}

const QUEUE_ROUTES: [5]OpHandler = [5]OpHandler{
    OpHandler{ op: serial_protocol.CMD_A, check: bang3, run: queue_push },
    OpHandler{ op: serial_protocol.CMD_D, check: bang23, run: queue_dequeue },
    OpHandler{ op: serial_protocol.CMD_C, check: bang23, run: queue_count },
    OpHandler{ op: serial_protocol.CMD_P, check: bang23, run: queue_peek },
    OpHandler{ op: serial_protocol.CMD_W, check: bang23, run: queue_wait }
}

func route_queue(s: ShellServiceState, m: service_effect.Message, op: u8) service_effect.Effect {
    return dispatch(QUEUE_ROUTES, s, m, op)
}

func file_create(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward_two(s, service_topology.FILE_ENDPOINT_ID, serial_protocol.CMD_C, m.payload[2])
}

func file_write(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward_three(s, service_topology.FILE_ENDPOINT_ID, serial_protocol.CMD_W, m.payload[2], m.payload[3])
}

func file_read(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward_two(s, service_topology.FILE_ENDPOINT_ID, serial_protocol.CMD_R, m.payload[2])
}

func file_list(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward_empty(s, service_topology.FILE_ENDPOINT_ID)
}

const FILE_ROUTES: [5]OpHandler = [5]OpHandler{
    OpHandler{ op: serial_protocol.CMD_C, check: bang3, run: file_create },
    OpHandler{ op: serial_protocol.CMD_W, check: anymsg, run: file_write },
    OpHandler{ op: serial_protocol.CMD_R, check: bang3, run: file_read },
    OpHandler{ op: serial_protocol.CMD_L, check: bang23, run: file_list },
    UNUSED_ROUTE
}

func route_file(s: ShellServiceState, m: service_effect.Message, op: u8) service_effect.Effect {
    return dispatch(FILE_ROUTES, s, m, op)
}

func object_create(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward_three(s, service_topology.OBJECT_STORE_ENDPOINT_ID, serial_protocol.CMD_C, m.payload[2], m.payload[3])
}

func object_read(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward_two(s, service_topology.OBJECT_STORE_ENDPOINT_ID, serial_protocol.CMD_R, m.payload[2])
}

func object_replace(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward_three(s, service_topology.OBJECT_STORE_ENDPOINT_ID, serial_protocol.CMD_W, m.payload[2], m.payload[3])
}

const OBJECT_ROUTES: [5]OpHandler = [5]OpHandler{
    OpHandler{ op: serial_protocol.CMD_C, check: anymsg, run: object_create },
    OpHandler{ op: serial_protocol.CMD_R, check: bang3, run: object_read },
    OpHandler{ op: serial_protocol.CMD_W, check: anymsg, run: object_replace },
    UNUSED_ROUTE,
    UNUSED_ROUTE
}

func route_object_store(s: ShellServiceState, m: service_effect.Message, op: u8) service_effect.Effect {
    return dispatch(OBJECT_ROUTES, s, m, op)
}

func timer_create(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward_three(s, service_topology.TIMER_ENDPOINT_ID, serial_protocol.CMD_C, m.payload[2], m.payload[3])
}

func timer_cancel(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward_two(s, service_topology.TIMER_ENDPOINT_ID, serial_protocol.CMD_X, m.payload[2])
}

func timer_query(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward_two(s, service_topology.TIMER_ENDPOINT_ID, serial_protocol.CMD_Q, m.payload[2])
}

func timer_expired(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward_two(s, service_topology.TIMER_ENDPOINT_ID, serial_protocol.CMD_E, m.payload[2])
}

const TIMER_ROUTES: [5]OpHandler = [5]OpHandler{
    OpHandler{ op: serial_protocol.CMD_C, check: anymsg, run: timer_create },
    OpHandler{ op: serial_protocol.CMD_X, check: bang3, run: timer_cancel },
    OpHandler{ op: serial_protocol.CMD_Q, check: bang3, run: timer_query },
    OpHandler{ op: serial_protocol.CMD_E, check: bang3, run: timer_expired },
    UNUSED_ROUTE
}

func route_timer(s: ShellServiceState, m: service_effect.Message, op: u8) service_effect.Effect {
    return dispatch(TIMER_ROUTES, s, m, op)
}

func journal_append(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward_three(s, service_topology.JOURNAL_ENDPOINT_ID, serial_protocol.CMD_A, m.payload[2], m.payload[3])
}

func journal_read(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward_two(s, service_topology.JOURNAL_ENDPOINT_ID, serial_protocol.CMD_R, m.payload[2])
}

func journal_clear(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward_two(s, service_topology.JOURNAL_ENDPOINT_ID, serial_protocol.CMD_C, m.payload[2])
}

const JOURNAL_ROUTES: [5]OpHandler = [5]OpHandler{
    OpHandler{ op: serial_protocol.CMD_A, check: anymsg, run: journal_append },
    OpHandler{ op: serial_protocol.CMD_R, check: bang3, run: journal_read },
    OpHandler{ op: serial_protocol.CMD_C, check: bang3, run: journal_clear },
    UNUSED_ROUTE,
    UNUSED_ROUTE
}

func route_journal(s: ShellServiceState, m: service_effect.Message, op: u8) service_effect.Effect {
    return dispatch(JOURNAL_ROUTES, s, m, op)
}

func task_submit(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward_two(s, service_topology.TASK_ENDPOINT_ID, serial_protocol.CMD_S, m.payload[2])
}

func task_query(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward_two(s, service_topology.TASK_ENDPOINT_ID, serial_protocol.CMD_Q, m.payload[2])
}

func task_cancel(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward_two(s, service_topology.TASK_ENDPOINT_ID, serial_protocol.CMD_C, m.payload[2])
}

func task_done(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward_two(s, service_topology.TASK_ENDPOINT_ID, serial_protocol.CMD_D, m.payload[2])
}

func task_list(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward_two(s, service_topology.TASK_ENDPOINT_ID, serial_protocol.CMD_L, m.payload[2])
}

const TASK_ROUTES: [5]OpHandler = [5]OpHandler{
    OpHandler{ op: serial_protocol.CMD_S, check: bang3, run: task_submit },
    OpHandler{ op: serial_protocol.CMD_Q, check: bang3, run: task_query },
    OpHandler{ op: serial_protocol.CMD_C, check: bang3, run: task_cancel },
    OpHandler{ op: serial_protocol.CMD_D, check: bang3, run: task_done },
    OpHandler{ op: serial_protocol.CMD_L, check: bang3, run: task_list }
}

func route_task(s: ShellServiceState, m: service_effect.Message, op: u8) service_effect.Effect {
    return dispatch(TASK_ROUTES, s, m, op)
}

func workflow_schedule(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward_three(s, service_topology.WORKFLOW_ENDPOINT_ID, serial_protocol.CMD_S, m.payload[2], m.payload[3])
}

func workflow_update(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward_three(s, service_topology.WORKFLOW_ENDPOINT_ID, serial_protocol.CMD_W, m.payload[2], m.payload[3])
}

func workflow_query(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward_two(s, service_topology.WORKFLOW_ENDPOINT_ID, serial_protocol.CMD_Q, m.payload[2])
}

func workflow_cancel(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward_two(s, service_topology.WORKFLOW_ENDPOINT_ID, serial_protocol.CMD_C, m.payload[2])
}

const WORKFLOW_ROUTES: [5]OpHandler = [5]OpHandler{
    OpHandler{ op: serial_protocol.CMD_S, check: anymsg, run: workflow_schedule },
    OpHandler{ op: serial_protocol.CMD_W, check: anymsg, run: workflow_update },
    OpHandler{ op: serial_protocol.CMD_Q, check: bang3, run: workflow_query },
    OpHandler{ op: serial_protocol.CMD_C, check: bang3, run: workflow_cancel },
    UNUSED_ROUTE
}

func route_workflow(s: ShellServiceState, m: service_effect.Message, op: u8) service_effect.Effect {
    return dispatch(WORKFLOW_ROUTES, s, m, op)
}

func completion_fetch(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward_two(s, service_topology.COMPLETION_MAILBOX_ENDPOINT_ID, serial_protocol.CMD_F, serial_protocol.CMD_BANG)
}

func completion_ack(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward_two(s, service_topology.COMPLETION_MAILBOX_ENDPOINT_ID, serial_protocol.CMD_A, serial_protocol.CMD_BANG)
}

func completion_count(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward_two(s, service_topology.COMPLETION_MAILBOX_ENDPOINT_ID, serial_protocol.CMD_C, serial_protocol.CMD_BANG)
}

const COMPLETION_ROUTES: [5]OpHandler = [5]OpHandler{
    OpHandler{ op: serial_protocol.CMD_F, check: bang23, run: completion_fetch },
    OpHandler{ op: serial_protocol.CMD_A, check: bang23, run: completion_ack },
    OpHandler{ op: serial_protocol.CMD_C, check: bang23, run: completion_count },
    UNUSED_ROUTE,
    UNUSED_ROUTE
}

func route_completion(s: ShellServiceState, m: service_effect.Message, op: u8) service_effect.Effect {
    return dispatch(COMPLETION_ROUTES, s, m, op)
}

func lease_issue(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward_two(s, service_topology.LEASE_ENDPOINT_ID, serial_protocol.CMD_I, m.payload[2])
}

func lease_consume(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    payload: [4]u8 = primitives.zero_payload()
    payload[0] = serial_protocol.CMD_U
    payload[1] = m.payload[2]
    payload[2] = m.payload[3]
    return forward_payload(s, service_topology.LEASE_ENDPOINT_ID, 3, payload)
}

const LEASE_ROUTES: [5]OpHandler = [5]OpHandler{
    OpHandler{ op: serial_protocol.CMD_I, check: bang3, run: lease_issue },
    OpHandler{ op: serial_protocol.CMD_U, check: anymsg, run: lease_consume },
    UNUSED_ROUTE,
    UNUSED_ROUTE,
    UNUSED_ROUTE
}

func route_lease(s: ShellServiceState, m: service_effect.Message, op: u8) service_effect.Effect {
    return dispatch(LEASE_ROUTES, s, m, op)
}

func ticket_issue(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward_empty(s, service_topology.TICKET_ENDPOINT_ID)
}

func ticket_use(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward_two(s, service_topology.TICKET_ENDPOINT_ID, m.payload[2], m.payload[3])
}

const TICKET_ROUTES: [5]OpHandler = [5]OpHandler{
    OpHandler{ op: serial_protocol.CMD_I, check: bang23, run: ticket_issue },
    OpHandler{ op: serial_protocol.CMD_U, check: anymsg, run: ticket_use },
    UNUSED_ROUTE,
    UNUSED_ROUTE,
    UNUSED_ROUTE
}

func route_ticket(s: ShellServiceState, m: service_effect.Message, op: u8) service_effect.Effect {
    return dispatch(TICKET_ROUTES, s, m, op)
}

func handle(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    cmd: u8 = m.payload[0]
    op: u8 = m.payload[1]

    if !shell_ready(s) || m.payload_len != 4 {
        return invalid_effect(SHELL_INVALID_SHAPE)
    }

    switch cmd {
    case serial_protocol.CMD_E:
        return route_echo(s, m, op)

    case serial_protocol.CMD_L:
        return route_log(s, m, op)

    case serial_protocol.CMD_K:
        return route_kv(s, m, op)

    case serial_protocol.CMD_X:
        return route_lifecycle(s, m, op)

    case serial_protocol.CMD_Q:
        return route_queue(s, m, op)

    case serial_protocol.CMD_F:
        return route_file(s, m, op)

    case serial_protocol.CMD_N:
        return route_object_store(s, m, op)

    case serial_protocol.CMD_M:
        return route_timer(s, m, op)

    case serial_protocol.CMD_Y:
        return route_journal(s, m, op)

    case serial_protocol.CMD_J:
        return route_task(s, m, op)

    case serial_protocol.CMD_O:
        return route_workflow(s, m, op)

    case serial_protocol.CMD_Z:
        return route_lease(s, m, op)

    case serial_protocol.CMD_V:
        return route_completion(s, m, op)

    case serial_protocol.CMD_T:
        return route_ticket(s, m, op)
    default:
        return invalid_effect(SHELL_INVALID_COMMAND)
    }
}
