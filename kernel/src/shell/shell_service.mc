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

func forward(s: ShellServiceState, endpoint: u32, send_len: usize, value0: u8, value1: u8, value2: u8) service_effect.Effect {
    payload: [4]u8 = primitives.zero_payload()
    if send_len > 0 {
        payload[0] = value0
    }
    if send_len > 1 {
        payload[1] = value1
    }
    if send_len > 2 {
        payload[2] = value2
    }
    return forward_payload(s, endpoint, send_len, payload)
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
    case serial_protocol.TARGET_WORKFLOW:
        return service_topology.WORKFLOW_ENDPOINT_ID
    case serial_protocol.TARGET_LEASE:
        return service_topology.LEASE_ENDPOINT_ID
    case serial_protocol.TARGET_COMPLETION:
        return service_topology.COMPLETION_MAILBOX_ENDPOINT_ID
    case serial_protocol.TARGET_OBJECT_STORE:
        return service_topology.OBJECT_STORE_ENDPOINT_ID
    case serial_protocol.TARGET_CONNECTION:
        return service_topology.CONNECTION_ENDPOINT_ID
    case serial_protocol.TARGET_UPDATE_STORE:
        return service_topology.UPDATE_STORE_ENDPOINT_ID
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

func lifecycle_payload_effect(status: syscall.SyscallStatus, payload: [4]u8) service_effect.Effect {
    return service_effect.effect_reply(status, 4, payload)
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
    return forward(s, service_topology.ECHO_ENDPOINT_ID, 2, m.payload[2], m.payload[3], 0)
}

func log_append(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward(s, service_topology.LOG_ENDPOINT_ID, 1, m.payload[2], 0, 0)
}

func log_tail(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward(s, service_topology.LOG_ENDPOINT_ID, 0, 0, 0, 0)
}

func route_log(s: ShellServiceState, m: service_effect.Message, op: u8) service_effect.Effect {
    switch op {
    case serial_protocol.CMD_A:
        if !bang3(m) {
            return invalid_effect(SHELL_INVALID_SHAPE)
        }
        return log_append(s, m)
    case serial_protocol.CMD_T:
        if !bang23(m) {
            return invalid_effect(SHELL_INVALID_SHAPE)
        }
        return log_tail(s, m)
    default:
        return invalid_effect(SHELL_INVALID_COMMAND)
    }
}

func kv_set(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward(s, service_topology.KV_ENDPOINT_ID, 2, m.payload[2], m.payload[3], 0)
}

func kv_get(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward(s, service_topology.KV_ENDPOINT_ID, 1, m.payload[2], 0, 0)
}

func kv_count(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward(s, service_topology.KV_ENDPOINT_ID, 0, 0, 0, 0)
}

func route_kv(s: ShellServiceState, m: service_effect.Message, op: u8) service_effect.Effect {
    switch op {
    case serial_protocol.CMD_S:
        return kv_set(s, m)
    case serial_protocol.CMD_G:
        if !bang3(m) {
            return invalid_effect(SHELL_INVALID_SHAPE)
        }
        return kv_get(s, m)
    case serial_protocol.CMD_C:
        if !bang23(m) {
            return invalid_effect(SHELL_INVALID_SHAPE)
        }
        return kv_count(s, m)
    default:
        return invalid_effect(SHELL_INVALID_COMMAND)
    }
}

func queue_push(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward(s, service_topology.QUEUE_ENDPOINT_ID, 1, m.payload[2], 0, 0)
}

func queue_dequeue(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward(s, service_topology.QUEUE_ENDPOINT_ID, 0, 0, 0, 0)
}

func queue_count(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward(s, service_topology.QUEUE_ENDPOINT_ID, 2, serial_protocol.CMD_C, serial_protocol.CMD_BANG, 0)
}

func queue_peek(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward(s, service_topology.QUEUE_ENDPOINT_ID, 2, serial_protocol.CMD_P, serial_protocol.CMD_BANG, 0)
}

func queue_wait(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward(s, service_topology.QUEUE_ENDPOINT_ID, 2, serial_protocol.CMD_W, serial_protocol.CMD_BANG, 0)
}

func route_queue(s: ShellServiceState, m: service_effect.Message, op: u8) service_effect.Effect {
    switch op {
    case serial_protocol.CMD_A:
        if !bang3(m) {
            return invalid_effect(SHELL_INVALID_SHAPE)
        }
        return queue_push(s, m)
    case serial_protocol.CMD_D:
        if !bang23(m) {
            return invalid_effect(SHELL_INVALID_SHAPE)
        }
        return queue_dequeue(s, m)
    case serial_protocol.CMD_C:
        if !bang23(m) {
            return invalid_effect(SHELL_INVALID_SHAPE)
        }
        return queue_count(s, m)
    case serial_protocol.CMD_P:
        if !bang23(m) {
            return invalid_effect(SHELL_INVALID_SHAPE)
        }
        return queue_peek(s, m)
    case serial_protocol.CMD_W:
        if !bang23(m) {
            return invalid_effect(SHELL_INVALID_SHAPE)
        }
        return queue_wait(s, m)
    default:
        return invalid_effect(SHELL_INVALID_COMMAND)
    }
}

func file_create(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward(s, service_topology.FILE_ENDPOINT_ID, 2, serial_protocol.CMD_C, m.payload[2], 0)
}

func file_write(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward(s, service_topology.FILE_ENDPOINT_ID, 3, serial_protocol.CMD_W, m.payload[2], m.payload[3])
}

func file_read(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward(s, service_topology.FILE_ENDPOINT_ID, 2, serial_protocol.CMD_R, m.payload[2], 0)
}

func file_list(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward(s, service_topology.FILE_ENDPOINT_ID, 0, 0, 0, 0)
}

func route_file(s: ShellServiceState, m: service_effect.Message, op: u8) service_effect.Effect {
    switch op {
    case serial_protocol.CMD_C:
        if !bang3(m) {
            return invalid_effect(SHELL_INVALID_SHAPE)
        }
        return file_create(s, m)
    case serial_protocol.CMD_W:
        return file_write(s, m)
    case serial_protocol.CMD_R:
        if !bang3(m) {
            return invalid_effect(SHELL_INVALID_SHAPE)
        }
        return file_read(s, m)
    case serial_protocol.CMD_L:
        if !bang23(m) {
            return invalid_effect(SHELL_INVALID_SHAPE)
        }
        return file_list(s, m)
    default:
        return invalid_effect(SHELL_INVALID_COMMAND)
    }
}

func object_create(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward(s, service_topology.OBJECT_STORE_ENDPOINT_ID, 3, serial_protocol.CMD_C, m.payload[2], m.payload[3])
}

func object_read(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward(s, service_topology.OBJECT_STORE_ENDPOINT_ID, 2, serial_protocol.CMD_R, m.payload[2], 0)
}

func object_replace(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward(s, service_topology.OBJECT_STORE_ENDPOINT_ID, 3, serial_protocol.CMD_W, m.payload[2], m.payload[3])
}

func route_object_store(s: ShellServiceState, m: service_effect.Message, op: u8) service_effect.Effect {
    switch op {
    case serial_protocol.CMD_C:
        return object_create(s, m)
    case serial_protocol.CMD_R:
        if !bang3(m) {
            return invalid_effect(SHELL_INVALID_SHAPE)
        }
        return object_read(s, m)
    case serial_protocol.CMD_W:
        return object_replace(s, m)
    default:
        return invalid_effect(SHELL_INVALID_COMMAND)
    }
}

func update_stage(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward(s, service_topology.UPDATE_STORE_ENDPOINT_ID, 2, serial_protocol.CMD_A, m.payload[2], 0)
}

func update_clear(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward(s, service_topology.UPDATE_STORE_ENDPOINT_ID, 2, serial_protocol.CMD_C, serial_protocol.CMD_BANG, 0)
}

func update_query(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward(s, service_topology.UPDATE_STORE_ENDPOINT_ID, 2, serial_protocol.CMD_Q, serial_protocol.CMD_BANG, 0)
}

func update_manifest(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward(s, service_topology.UPDATE_STORE_ENDPOINT_ID, 3, serial_protocol.CMD_M, m.payload[2], m.payload[3])
}

func route_update_store(s: ShellServiceState, m: service_effect.Message, op: u8) service_effect.Effect {
    switch op {
    case serial_protocol.CMD_A:
        if !bang3(m) {
            return invalid_effect(SHELL_INVALID_SHAPE)
        }
        return update_stage(s, m)
    case serial_protocol.CMD_C:
        if !bang23(m) {
            return invalid_effect(SHELL_INVALID_SHAPE)
        }
        return update_clear(s, m)
    case serial_protocol.CMD_Q:
        if !bang23(m) {
            return invalid_effect(SHELL_INVALID_SHAPE)
        }
        return update_query(s, m)
    case serial_protocol.CMD_M:
        return update_manifest(s, m)
    default:
        return invalid_effect(SHELL_INVALID_COMMAND)
    }
}

func timer_create(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward(s, service_topology.TIMER_ENDPOINT_ID, 3, serial_protocol.CMD_C, m.payload[2], m.payload[3])
}

func timer_cancel(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward(s, service_topology.TIMER_ENDPOINT_ID, 2, serial_protocol.CMD_X, m.payload[2], 0)
}

func timer_query(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward(s, service_topology.TIMER_ENDPOINT_ID, 2, serial_protocol.CMD_Q, m.payload[2], 0)
}

func timer_expired(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward(s, service_topology.TIMER_ENDPOINT_ID, 2, serial_protocol.CMD_E, m.payload[2], 0)
}

func route_timer(s: ShellServiceState, m: service_effect.Message, op: u8) service_effect.Effect {
    switch op {
    case serial_protocol.CMD_C:
        return timer_create(s, m)
    case serial_protocol.CMD_X:
        if !bang3(m) {
            return invalid_effect(SHELL_INVALID_SHAPE)
        }
        return timer_cancel(s, m)
    case serial_protocol.CMD_Q:
        if !bang3(m) {
            return invalid_effect(SHELL_INVALID_SHAPE)
        }
        return timer_query(s, m)
    case serial_protocol.CMD_E:
        if !bang3(m) {
            return invalid_effect(SHELL_INVALID_SHAPE)
        }
        return timer_expired(s, m)
    default:
        return invalid_effect(SHELL_INVALID_COMMAND)
    }
}

func journal_append(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward(s, service_topology.JOURNAL_ENDPOINT_ID, 3, serial_protocol.CMD_A, m.payload[2], m.payload[3])
}

func journal_read(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward(s, service_topology.JOURNAL_ENDPOINT_ID, 2, serial_protocol.CMD_R, m.payload[2], 0)
}

func journal_clear(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward(s, service_topology.JOURNAL_ENDPOINT_ID, 2, serial_protocol.CMD_C, m.payload[2], 0)
}

func route_journal(s: ShellServiceState, m: service_effect.Message, op: u8) service_effect.Effect {
    switch op {
    case serial_protocol.CMD_A:
        return journal_append(s, m)
    case serial_protocol.CMD_R:
        if !bang3(m) {
            return invalid_effect(SHELL_INVALID_SHAPE)
        }
        return journal_read(s, m)
    case serial_protocol.CMD_C:
        if !bang3(m) {
            return invalid_effect(SHELL_INVALID_SHAPE)
        }
        return journal_clear(s, m)
    default:
        return invalid_effect(SHELL_INVALID_COMMAND)
    }
}

func task_submit(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward(s, service_topology.TASK_ENDPOINT_ID, 2, serial_protocol.CMD_S, m.payload[2], 0)
}

func task_query(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward(s, service_topology.TASK_ENDPOINT_ID, 2, serial_protocol.CMD_Q, m.payload[2], 0)
}

func task_cancel(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward(s, service_topology.TASK_ENDPOINT_ID, 2, serial_protocol.CMD_C, m.payload[2], 0)
}

func task_done(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward(s, service_topology.TASK_ENDPOINT_ID, 2, serial_protocol.CMD_D, m.payload[2], 0)
}

func task_list(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward(s, service_topology.TASK_ENDPOINT_ID, 2, serial_protocol.CMD_L, m.payload[2], 0)
}

func route_task(s: ShellServiceState, m: service_effect.Message, op: u8) service_effect.Effect {
    switch op {
    case serial_protocol.CMD_S:
        if !bang3(m) {
            return invalid_effect(SHELL_INVALID_SHAPE)
        }
        return task_submit(s, m)
    case serial_protocol.CMD_Q:
        if !bang3(m) {
            return invalid_effect(SHELL_INVALID_SHAPE)
        }
        return task_query(s, m)
    case serial_protocol.CMD_C:
        if !bang3(m) {
            return invalid_effect(SHELL_INVALID_SHAPE)
        }
        return task_cancel(s, m)
    case serial_protocol.CMD_D:
        if !bang3(m) {
            return invalid_effect(SHELL_INVALID_SHAPE)
        }
        return task_done(s, m)
    case serial_protocol.CMD_L:
        if !bang3(m) {
            return invalid_effect(SHELL_INVALID_SHAPE)
        }
        return task_list(s, m)
    default:
        return invalid_effect(SHELL_INVALID_COMMAND)
    }
}

func workflow_schedule(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward(s, service_topology.WORKFLOW_ENDPOINT_ID, 3, serial_protocol.CMD_S, m.payload[2], m.payload[3])
}

func workflow_update(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward(s, service_topology.WORKFLOW_ENDPOINT_ID, 3, serial_protocol.CMD_W, m.payload[2], m.payload[3])
}

func workflow_apply_update(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward(s, service_topology.WORKFLOW_ENDPOINT_ID, 1, serial_protocol.CMD_A, 0, 0)
}

func workflow_apply_update_lease(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward(s, service_topology.WORKFLOW_ENDPOINT_ID, 2, serial_protocol.CMD_L, m.payload[2], 0)
}

func workflow_query(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward(s, service_topology.WORKFLOW_ENDPOINT_ID, 2, serial_protocol.CMD_Q, m.payload[2], 0)
}

func workflow_cancel(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward(s, service_topology.WORKFLOW_ENDPOINT_ID, 2, serial_protocol.CMD_C, m.payload[2], 0)
}

func route_workflow(s: ShellServiceState, m: service_effect.Message, op: u8) service_effect.Effect {
    switch op {
    case serial_protocol.CMD_A:
        if !bang23(m) {
            return invalid_effect(SHELL_INVALID_SHAPE)
        }
        return workflow_apply_update(s, m)
    case serial_protocol.CMD_S:
        return workflow_schedule(s, m)
    case serial_protocol.CMD_W:
        return workflow_update(s, m)
    case serial_protocol.CMD_Q:
        if !bang3(m) {
            return invalid_effect(SHELL_INVALID_SHAPE)
        }
        return workflow_query(s, m)
    case serial_protocol.CMD_C:
        if !bang3(m) {
            return invalid_effect(SHELL_INVALID_SHAPE)
        }
        return workflow_cancel(s, m)
    case serial_protocol.CMD_L:
        if !bang3(m) {
            return invalid_effect(SHELL_INVALID_SHAPE)
        }
        return workflow_apply_update_lease(s, m)
    default:
        return invalid_effect(SHELL_INVALID_COMMAND)
    }
}

func completion_fetch(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward(s, service_topology.COMPLETION_MAILBOX_ENDPOINT_ID, 2, serial_protocol.CMD_F, serial_protocol.CMD_BANG, 0)
}

func completion_ack(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward(s, service_topology.COMPLETION_MAILBOX_ENDPOINT_ID, 2, serial_protocol.CMD_A, serial_protocol.CMD_BANG, 0)
}

func completion_count(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward(s, service_topology.COMPLETION_MAILBOX_ENDPOINT_ID, 2, serial_protocol.CMD_C, serial_protocol.CMD_BANG, 0)
}

func route_completion(s: ShellServiceState, m: service_effect.Message, op: u8) service_effect.Effect {
    switch op {
    case serial_protocol.CMD_F:
        if !bang23(m) {
            return invalid_effect(SHELL_INVALID_SHAPE)
        }
        return completion_fetch(s, m)
    case serial_protocol.CMD_A:
        if !bang23(m) {
            return invalid_effect(SHELL_INVALID_SHAPE)
        }
        return completion_ack(s, m)
    case serial_protocol.CMD_C:
        if !bang23(m) {
            return invalid_effect(SHELL_INVALID_SHAPE)
        }
        return completion_count(s, m)
    default:
        return invalid_effect(SHELL_INVALID_COMMAND)
    }
}

func connection_open(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward(s, service_topology.CONNECTION_ENDPOINT_ID, 3, serial_protocol.CMD_O, m.payload[2], m.payload[3])
}

func connection_receive(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward(s, service_topology.CONNECTION_ENDPOINT_ID, 3, serial_protocol.CMD_R, m.payload[2], m.payload[3])
}

func connection_send(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward(s, service_topology.CONNECTION_ENDPOINT_ID, 3, serial_protocol.CMD_S, m.payload[2], m.payload[3])
}

func connection_close(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward(s, service_topology.CONNECTION_ENDPOINT_ID, 3, serial_protocol.CMD_C, m.payload[2], m.payload[3])
}

func connection_execute(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward(s, service_topology.CONNECTION_ENDPOINT_ID, 2, serial_protocol.CMD_X, m.payload[2], 0)
}

func route_connection(s: ShellServiceState, m: service_effect.Message, op: u8) service_effect.Effect {
    switch op {
    case serial_protocol.CMD_O:
        return connection_open(s, m)
    case serial_protocol.CMD_R:
        return connection_receive(s, m)
    case serial_protocol.CMD_S:
        return connection_send(s, m)
    case serial_protocol.CMD_C:
        return connection_close(s, m)
    case serial_protocol.CMD_X:
        if !bang3(m) {
            return invalid_effect(SHELL_INVALID_SHAPE)
        }
        return connection_execute(s, m)
    default:
        return invalid_effect(SHELL_INVALID_COMMAND)
    }
}

func lease_issue(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward(s, service_topology.LEASE_ENDPOINT_ID, 2, serial_protocol.CMD_I, m.payload[2], 0)
}

func lease_issue_installer_apply(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward(s, service_topology.LEASE_ENDPOINT_ID, 1, serial_protocol.CMD_A, 0, 0)
}

func lease_consume(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    payload: [4]u8 = primitives.zero_payload()
    payload[0] = serial_protocol.CMD_U
    payload[1] = m.payload[2]
    payload[2] = m.payload[3]
    return forward_payload(s, service_topology.LEASE_ENDPOINT_ID, 3, payload)
}

func lease_issue_object_update(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward(s, service_topology.LEASE_ENDPOINT_ID, 3, serial_protocol.CMD_W, m.payload[2], m.payload[3])
}

func lease_issue_external_ticket(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward(s, service_topology.LEASE_ENDPOINT_ID, 3, serial_protocol.CMD_T, m.payload[2], m.payload[3])
}

func lease_consume_object_update(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward(s, service_topology.LEASE_ENDPOINT_ID, 2, serial_protocol.CMD_D, m.payload[2], 0)
}

func route_lease(s: ShellServiceState, m: service_effect.Message, op: u8) service_effect.Effect {
    switch op {
    case serial_protocol.CMD_A:
        if !bang23(m) {
            return invalid_effect(SHELL_INVALID_SHAPE)
        }
        return lease_issue_installer_apply(s, m)
    case serial_protocol.CMD_I:
        if !bang3(m) {
            return invalid_effect(SHELL_INVALID_SHAPE)
        }
        return lease_issue(s, m)
    case serial_protocol.CMD_U:
        return lease_consume(s, m)
    case serial_protocol.CMD_W:
        return lease_issue_object_update(s, m)
    case serial_protocol.CMD_D:
        if !bang3(m) {
            return invalid_effect(SHELL_INVALID_SHAPE)
        }
        return lease_consume_object_update(s, m)
    case serial_protocol.CMD_T:
        return lease_issue_external_ticket(s, m)
    default:
        return invalid_effect(SHELL_INVALID_COMMAND)
    }
}

func ticket_issue(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward(s, service_topology.TICKET_ENDPOINT_ID, 0, 0, 0, 0)
}

func ticket_use(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    return forward(s, service_topology.TICKET_ENDPOINT_ID, 2, m.payload[2], m.payload[3], 0)
}

func route_ticket(s: ShellServiceState, m: service_effect.Message, op: u8) service_effect.Effect {
    switch op {
    case serial_protocol.CMD_I:
        if !bang23(m) {
            return invalid_effect(SHELL_INVALID_SHAPE)
        }
        return ticket_issue(s, m)
    case serial_protocol.CMD_U:
        return ticket_use(s, m)
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

    case serial_protocol.CMD_H:
        return route_connection(s, m, op)
    case serial_protocol.CMD_B:
        return route_update_store(s, m, op)
    default:
        return invalid_effect(SHELL_INVALID_COMMAND)
    }
}
