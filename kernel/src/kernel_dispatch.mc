// Dispatch and routing layer for the kernel boot state.
//
// This module owns all runtime dispatch logic: leaf service routing,
// the effect-chasing loop, and the serial path sequence.
//
// boot.mc owns configuration, state construction, and service refs.
// This module owns how an incoming observation is turned into an Effect.

import boot
import completion_mailbox_service
import connection_service
import echo_service
import file_service
import journal_service
import init
import identity_taxonomy
import event_codes
import kv_service
import lease_service
import log_service
import object_store_service
import primitives
import queue_service
import serial_protocol
import serial_shell_path
import service_effect
import service_state
import service_topology
import shell_service
import syscall
import task_service
import ticket_service
import timer_service
import transfer_grant
import transfer_service
import workflow_service

// MAX_EFFECT_CHAIN_DEPTH guards against send loops between services.
// If the chain exceeds this depth the original caller receives Exhausted.
// Value 8 is well above the current topology (serial → shell → kv/log, depth ≤ 3).
const MAX_EFFECT_CHAIN_DEPTH: u32 = 8
const KV_WRITE_LOG_MARKER: u8 = 75

struct LifecycleRoute {
    op: u8
    reply: func(*boot.KernelBootState, u8) service_effect.Effect
}

struct LeafRoute {
    endpoint: u32
    reply: func(*boot.KernelBootState, service_effect.Message) service_effect.Effect
}

func effect_to_message(effect: service_effect.Effect) service_effect.Message {
    return service_effect.message(
        service_effect.effect_send_source_pid(effect),
        service_effect.effect_send_endpoint_id(effect),
        service_effect.effect_send_payload_len(effect),
        service_effect.effect_send_payload(effect)
    )
}

func lifecycle_is_lane_target(target: u8) bool {
    return identity_taxonomy.identity_target_is_lane(target)
}

func lifecycle_target_endpoint(target: u8) u32 {
    return shell_service.lifecycle_target_endpoint(target)
}

func authority_class_code(class: service_topology.ServiceAuthorityClass) u8 {
    switch class {
    case service_topology.ServiceAuthorityClass.PublicEndpoint:
        return serial_protocol.AUTHORITY_PUBLIC
    case service_topology.ServiceAuthorityClass.TransferOnly:
        return serial_protocol.AUTHORITY_TRANSFER
    case service_topology.ServiceAuthorityClass.RetainedOwner:
        return serial_protocol.AUTHORITY_RETAINED
    case service_topology.ServiceAuthorityClass.DurableOwner:
        return serial_protocol.AUTHORITY_DURABLE
    case service_topology.ServiceAuthorityClass.ShellControl:
        return serial_protocol.AUTHORITY_SHELL
    default:
        return 0
    }
}

func authority_payload(target: u8, class: u8, transfer: u8, scope: u8) [4]u8 {
    payload: [4]u8 = primitives.zero_payload()
    payload[0] = target
    payload[1] = class
    payload[2] = transfer
    payload[3] = scope
    return payload
}

func lifecycle_policy_code(target: u8) u8 {
    info: init.RestartPolicyInfo = init.restart_policy_for_target(target)
    return service_state.state_policy_code(info.policy)
}

func lifecycle_authority_reply(state: *boot.KernelBootState, target: u8) service_effect.Effect {
    if lifecycle_is_lane_target(target) {
        return shell_service.lifecycle_authority_effect(
            syscall.SyscallStatus.Ok,
            authority_payload(
                target,
                serial_protocol.AUTHORITY_COORDINATED,
                serial_protocol.AUTHORITY_TRANSFER_NO,
                serial_protocol.AUTHORITY_SCOPE_COORDINATED))
    }

    authority_endpoint: u32 = lifecycle_target_endpoint(target)
    if authority_endpoint == 0 {
        return shell_service.invalid_effect(shell_service.SHELL_INVALID_COMMAND)
    }

    class: service_topology.ServiceAuthorityClass = service_topology.service_authority_class(authority_endpoint)
    class_code: u8 = authority_class_code(class)
    if class_code == 0 {
        return shell_service.invalid_effect(shell_service.SHELL_INVALID_COMMAND)
    }

    transfer: u8 = serial_protocol.AUTHORITY_TRANSFER_NO
    if class == service_topology.ServiceAuthorityClass.TransferOnly {
        transfer = serial_protocol.AUTHORITY_TRANSFER_YES
    }

    scope: u8 = serial_protocol.AUTHORITY_SCOPE_NONE
    if class == service_topology.ServiceAuthorityClass.RetainedOwner {
        scope = serial_protocol.AUTHORITY_SCOPE_RETAINED
    }
    if class == service_topology.ServiceAuthorityClass.DurableOwner {
        scope = serial_protocol.AUTHORITY_SCOPE_DURABLE
    }

    return shell_service.lifecycle_authority_effect(
        syscall.SyscallStatus.Ok,
        authority_payload(target, class_code, transfer, scope))
}

func lifecycle_state_reply(state: *boot.KernelBootState, target: u8) service_effect.Effect {
    if !lifecycle_is_lane_target(target) {
        state_endpoint: u32 = lifecycle_target_endpoint(target)
        if state_endpoint == 0 {
            return shell_service.invalid_effect(shell_service.SHELL_INVALID_COMMAND)
        }
    }

    mode: u8 = serial_protocol.LIFECYCLE_NONE
    if lifecycle_is_lane_target(target) {
        mode = serial_protocol.LIFECYCLE_RELOAD
    } else {
        mode = shell_service.lifecycle_mode(lifecycle_target_endpoint(target))
    }

    payload: [4]u8 = service_state.state_payload(
        target,
        service_state.state_class(target),
        service_state.state_mode_code(mode),
        service_state.state_participation(target),
        lifecycle_policy_code(target),
        boot.bootstate_metadata_for_target(*state, target),
        boot.bootstate_generation_marker_for_target(*state, target))
    return shell_service.lifecycle_state_effect(syscall.SyscallStatus.Ok, payload)
}

func lifecycle_durability_reply(state: *boot.KernelBootState, target: u8) service_effect.Effect {
    if !lifecycle_is_lane_target(target) {
        durability_endpoint: u32 = lifecycle_target_endpoint(target)
        if durability_endpoint == 0 {
            return shell_service.invalid_effect(shell_service.SHELL_INVALID_COMMAND)
        }
    }
    return shell_service.lifecycle_durability_effect(syscall.SyscallStatus.Ok, service_state.state_durability_payload(target))
}

func lifecycle_identity_reply(state: *boot.KernelBootState, target: u8) service_effect.Effect {
    if !identity_taxonomy.identity_target_has_generation_query(target) {
        return shell_service.invalid_effect(shell_service.SHELL_INVALID_COMMAND)
    }
    if target != serial_protocol.TARGET_WORKSET {
        identity_endpoint: u32 = lifecycle_target_endpoint(target)
        if identity_endpoint == 0 {
            return shell_service.invalid_effect(shell_service.SHELL_INVALID_COMMAND)
        }
    }
    payload: [4]u8 = boot.bootgeneration_payload_for_target(*state, target)
    return shell_service.lifecycle_identity_effect(syscall.SyscallStatus.Ok, payload)
}

func lifecycle_policy_reply(state: *boot.KernelBootState, target: u8) service_effect.Effect {
    payload: [4]u8 = init.restart_policy_payload(target)
    if payload[0] == 0 {
        return shell_service.invalid_effect(shell_service.SHELL_INVALID_COMMAND)
    }
    return shell_service.lifecycle_policy_effect(syscall.SyscallStatus.Ok, payload)
}

func lifecycle_summary_reply(state: *boot.KernelBootState, target: u8) service_effect.Effect {
    if lifecycle_is_lane_target(target) {
        return shell_service.lifecycle_summary_effect(syscall.SyscallStatus.Ok, boot.bootsummary_payload_for_target(*state, target))
    }
    summary_endpoint: u32 = lifecycle_target_endpoint(target)
    if summary_endpoint == 0 || !service_topology.service_can_restart(summary_endpoint) {
        return shell_service.invalid_effect(shell_service.SHELL_INVALID_COMMAND)
    }
    return shell_service.lifecycle_summary_effect(syscall.SyscallStatus.Ok, boot.bootsummary_payload_for_endpoint(*state, summary_endpoint))
}

func lifecycle_compare_reply(state: *boot.KernelBootState, target: u8) service_effect.Effect {
    if lifecycle_is_lane_target(target) {
        return shell_service.lifecycle_summary_effect(syscall.SyscallStatus.Ok, boot.bootcomparison_payload_for_target(*state, target))
    }
    compare_endpoint: u32 = lifecycle_target_endpoint(target)
    if compare_endpoint == 0 || !service_topology.service_can_restart(compare_endpoint) {
        return shell_service.invalid_effect(shell_service.SHELL_INVALID_COMMAND)
    }
    return shell_service.lifecycle_summary_effect(syscall.SyscallStatus.Ok, boot.bootcomparison_payload_for_target(*state, target))
}

func lifecycle_restart_reply(state: *boot.KernelBootState, target: u8) service_effect.Effect {
    if target == serial_protocol.TARGET_AUDIT {
        *state = init.restart_retained_audit_lane(*state)
        return shell_service.lifecycle_effect(syscall.SyscallStatus.Ok, target, serial_protocol.LIFECYCLE_RELOAD)
    }
    if target == serial_protocol.TARGET_WORKSET {
        *state = init.restart_retained_workset(*state)
        return shell_service.lifecycle_effect(syscall.SyscallStatus.Ok, target, serial_protocol.LIFECYCLE_RELOAD)
    }

    restart_endpoint: u32 = lifecycle_target_endpoint(target)
    if restart_endpoint == 0 {
        return shell_service.invalid_effect(shell_service.SHELL_INVALID_COMMAND)
    }
    if !service_topology.service_can_restart(restart_endpoint) {
        return shell_service.lifecycle_effect(syscall.SyscallStatus.InvalidArgument, target, shell_service.lifecycle_mode(restart_endpoint))
    }
    *state = init.restart(*state, restart_endpoint)
    return shell_service.lifecycle_effect(syscall.SyscallStatus.Ok, target, shell_service.lifecycle_mode(restart_endpoint))
}

func dispatch_log(state: *boot.KernelBootState, msg: service_effect.Message) service_effect.Effect {
    current: boot.KernelBootState = *state
    log_result: log_service.LogResult = log_service.handle(current.log.state, msg)
    *state = boot.bootwith_log(current, log_result.state)
    return log_result.effect
}

func dispatch_queue(state: *boot.KernelBootState, msg: service_effect.Message) service_effect.Effect {
    current: boot.KernelBootState = *state
    queue_result: queue_service.QueueResult = queue_service.handle(current.queue.state, msg)
    *state = boot.bootwith_queue(current, queue_result.state)
    return queue_result.effect
}

func dispatch_echo(state: *boot.KernelBootState, msg: service_effect.Message) service_effect.Effect {
    current: boot.KernelBootState = *state
    echo_result: echo_service.EchoResult = echo_service.handle(current.echo.state, msg)
    *state = boot.bootwith_echo(current, echo_result.state)
    return echo_result.effect
}

func dispatch_transfer(state: *boot.KernelBootState, msg: service_effect.Message) service_effect.Effect {
    current: boot.KernelBootState = *state
    transfer_result: transfer_service.TransferResult = transfer_service.handle(current.transfer.state, msg)
    *state = boot.bootwith_transfer(current, transfer_result.state)
    return transfer_result.effect
}

func dispatch_ticket(state: *boot.KernelBootState, msg: service_effect.Message) service_effect.Effect {
    current: boot.KernelBootState = *state
    ticket_result: ticket_service.TicketResult = ticket_service.handle(current.ticket.state, msg)
    *state = boot.bootwith_ticket(current, ticket_result.state)
    return ticket_result.effect
}

func dispatch_file(state: *boot.KernelBootState, msg: service_effect.Message) service_effect.Effect {
    current: boot.KernelBootState = *state
    file_result: file_service.FileResult = file_service.handle(current.file.state, msg)
    *state = boot.bootwith_file(current, file_result.state)
    return file_result.effect
}

func dispatch_timer(state: *boot.KernelBootState, msg: service_effect.Message) service_effect.Effect {
    current: boot.KernelBootState = *state
    timer_result: timer_service.TimerResult = timer_service.handle(current.timer.state, msg)
    *state = boot.bootwith_timer(current, timer_result.state)
    return timer_result.effect
}

func dispatch_task(state: *boot.KernelBootState, msg: service_effect.Message) service_effect.Effect {
    current: boot.KernelBootState = *state
    task_result: task_service.TaskResult = task_service.handle(current.task.state, msg)
    *state = boot.bootwith_task(current, task_result.state)
    return task_result.effect
}

func dispatch_connection(state: *boot.KernelBootState, msg: service_effect.Message) service_effect.Effect {
    current: boot.KernelBootState = *state
    if msg.payload_len == 2 && msg.payload[0] == connection_service.CONNECTION_OP_EXECUTE {
        prepared := connection_service.connection_execute_prepare(current.connection.state, msg.payload[1])
        if service_effect.effect_has_reply(prepared.effect) == 1 {
            return prepared.effect
        }
        step := workflow_service.workflow_step_schedule_connection(current.workflow.state, current.timer.state, current.task.state, current.object_store.state, current.journal.state, current.completion.state, prepared.request, msg.payload[1], prepared.opcode, u8(current.workset_generation))
        if object_store_service.object_store_changed(current.object_store.state, step.object_store) {
            if !object_store_service.object_store_persist(step.object_store) {
                return service_effect.effect_reply(syscall.SyscallStatus.Closed, 0, primitives.zero_payload())
            }
        }
        if journal_service.journal_changed(current.journal.state, step.journal) {
            if !journal_service.journal_persist(step.journal) {
                return service_effect.effect_reply(syscall.SyscallStatus.Closed, 0, primitives.zero_payload())
            }
        }
        if service_effect.effect_reply_status(step.effect) != syscall.SyscallStatus.Ok {
            return step.effect
        }
        next := boot.bootwith_connection(current, prepared.state)
        next = boot.bootwith_workflow(next, step.workflow)
        next = boot.bootwith_timer(next, step.timer)
        next = boot.bootwith_task(next, step.task)
        next = boot.bootwith_object_store(next, step.object_store)
        next = boot.bootwith_journal(next, step.journal)
        next = boot.bootwith_completion(next, step.completion)
        *state = next
        return step.effect
    }
    connection_result: connection_service.ConnectionResult = connection_service.handle(current.connection.state, msg)
    *state = boot.bootwith_connection(current, connection_result.state)
    return connection_result.effect
}

func dispatch_journal(state: *boot.KernelBootState, msg: service_effect.Message) service_effect.Effect {
    current: boot.KernelBootState = *state
    journal_result: journal_service.JournalResult = journal_service.handle(current.journal.state, msg)
    if journal_service.journal_changed(current.journal.state, journal_result.state) {
        if !journal_service.journal_persist(journal_result.state) {
            return service_effect.effect_reply(syscall.SyscallStatus.Closed, 0, primitives.zero_payload())
        }
    }
    *state = boot.bootwith_journal(current, journal_result.state)
    return journal_result.effect
}

func dispatch_object_store(state: *boot.KernelBootState, msg: service_effect.Message) service_effect.Effect {
    current: boot.KernelBootState = *state
    result := object_store_service.handle(current.object_store.state, msg)
    if object_store_service.object_store_changed(current.object_store.state, result.state) {
        if !object_store_service.object_store_persist(result.state) {
            return service_effect.effect_reply(syscall.SyscallStatus.Closed, 0, primitives.zero_payload())
        }
    }
    *state = boot.bootwith_object_store(current, result.state)
    return result.effect
}

func dispatch_completion_mailbox(state: *boot.KernelBootState, msg: service_effect.Message) service_effect.Effect {
    current: boot.KernelBootState = *state
    result: completion_mailbox_service.CompletionMailboxResult = completion_mailbox_service.handle(current.completion.state, msg)
    *state = boot.bootwith_completion(current, result.state)
    return result.effect
}

func dispatch_lease(state: *boot.KernelBootState, msg: service_effect.Message) service_effect.Effect {
    current: boot.KernelBootState = *state
    lease_result := lease_service.handle(current.lease.state, msg, u8(current.completion.generation), u8(current.workset_generation))
    next := boot.bootwith_lease(current, lease_result.state)
    if lease_result.op == lease_service.LEASE_OP_NONE {
        *state = next
        return lease_result.effect
    }

    if lease_result.op == lease_service.LEASE_OP_SCHEDULE_OBJECT_UPDATE {
        payload := primitives.zero_payload()
        payload[0] = workflow_service.WORKFLOW_OP_UPDATE
        payload[1] = lease_result.first
        payload[2] = lease_result.second
        *state = next
        delegated := service_effect.message(msg.source_pid, service_topology.WORKFLOW_ENDPOINT_ID, 3, payload)
        return dispatch_workflow(state, delegated)
    }

    completion_result := completion_mailbox_service.completion_mailbox_take(current.completion.state, lease_result.first)
    next = boot.bootwith_completion(next, completion_result.state)
    *state = next
    return completion_result.effect
}

func dispatch_workflow(state: *boot.KernelBootState, msg: service_effect.Message) service_effect.Effect {
    current: boot.KernelBootState = *state
    previous_workflow := current.workflow.state
    step := workflow_service.step(current.workflow.state, current.timer.state, current.task.state, current.object_store.state, current.journal.state, current.completion.state, current.connection.state, msg, u8(current.workset_generation))
    if object_store_service.object_store_changed(current.object_store.state, step.object_store) {
        if !object_store_service.object_store_persist(step.object_store) {
            return service_effect.effect_reply(syscall.SyscallStatus.Closed, 0, primitives.zero_payload())
        }
    }
    if journal_service.journal_changed(current.journal.state, step.journal) {
        if !journal_service.journal_persist(step.journal) {
            return service_effect.effect_reply(syscall.SyscallStatus.Closed, 0, primitives.zero_payload())
        }
    }
    next := boot.bootwith_workflow(current, step.workflow)
    next = boot.bootwith_timer(next, step.timer)
    next = boot.bootwith_task(next, step.task)
    next = boot.bootwith_object_store(next, step.object_store)
    next = boot.bootwith_journal(next, step.journal)
    next = boot.bootwith_completion(next, step.completion)
    if workflow_service.workflow_is_connection(previous_workflow) && !workflow_service.workflow_is_active(step.workflow) && previous_workflow.id != 0 {
        next = boot.bootwith_connection(next, connection_service.connection_request_finish(next.connection.state, workflow_service.workflow_connection_slot(previous_workflow), previous_workflow.id))
    }
    *state = next
    return step.effect
}

func lifecycle_invalid_reply(state: *boot.KernelBootState, target: u8) service_effect.Effect {
    return shell_service.invalid_effect(shell_service.SHELL_INVALID_COMMAND)
}

func lifecycle_route(op: u8) LifecycleRoute {
    switch op {
    case serial_protocol.CMD_A:
        return LifecycleRoute{ op: op, reply: lifecycle_authority_reply }
    case serial_protocol.CMD_C:
        return LifecycleRoute{ op: op, reply: lifecycle_state_reply }
    case serial_protocol.CMD_D:
        return LifecycleRoute{ op: op, reply: lifecycle_durability_reply }
    case serial_protocol.CMD_I:
        return LifecycleRoute{ op: op, reply: lifecycle_identity_reply }
    case serial_protocol.CMD_P:
        return LifecycleRoute{ op: op, reply: lifecycle_policy_reply }
    case serial_protocol.CMD_S:
        return LifecycleRoute{ op: op, reply: lifecycle_summary_reply }
    case serial_protocol.CMD_M:
        return LifecycleRoute{ op: op, reply: lifecycle_compare_reply }
    case serial_protocol.CMD_R:
        return LifecycleRoute{ op: op, reply: lifecycle_restart_reply }
    default:
        return LifecycleRoute{ op: op, reply: lifecycle_invalid_reply }
    }
}

func dispatch_invalid_endpoint(state: *boot.KernelBootState, msg: service_effect.Message) service_effect.Effect {
    return service_effect.effect_reply(syscall.SyscallStatus.InvalidEndpoint, 0, primitives.zero_payload())
}

func leaf_route(endpoint: u32) LeafRoute {
    switch endpoint {
    case service_topology.SHELL_ENDPOINT_ID:
        return LeafRoute{ endpoint: endpoint, reply: kernel_dispatch_shell_control }
    case service_topology.LOG_ENDPOINT_ID:
        return LeafRoute{ endpoint: endpoint, reply: dispatch_log }
    case service_topology.KV_ENDPOINT_ID:
        return LeafRoute{ endpoint: endpoint, reply: kernel_dispatch_kv }
    case service_topology.QUEUE_ENDPOINT_ID:
        return LeafRoute{ endpoint: endpoint, reply: dispatch_queue }
    case service_topology.ECHO_ENDPOINT_ID:
        return LeafRoute{ endpoint: endpoint, reply: dispatch_echo }
    case service_topology.TRANSFER_ENDPOINT_ID:
        return LeafRoute{ endpoint: endpoint, reply: dispatch_transfer }
    case service_topology.TICKET_ENDPOINT_ID:
        return LeafRoute{ endpoint: endpoint, reply: dispatch_ticket }
    case service_topology.FILE_ENDPOINT_ID:
        return LeafRoute{ endpoint: endpoint, reply: dispatch_file }
    case service_topology.TIMER_ENDPOINT_ID:
        return LeafRoute{ endpoint: endpoint, reply: dispatch_timer }
    case service_topology.TASK_ENDPOINT_ID:
        return LeafRoute{ endpoint: endpoint, reply: dispatch_task }
    case service_topology.CONNECTION_ENDPOINT_ID:
        return LeafRoute{ endpoint: endpoint, reply: dispatch_connection }
    case service_topology.JOURNAL_ENDPOINT_ID:
        return LeafRoute{ endpoint: endpoint, reply: dispatch_journal }
    case service_topology.WORKFLOW_ENDPOINT_ID:
        return LeafRoute{ endpoint: endpoint, reply: dispatch_workflow }
    case service_topology.LEASE_ENDPOINT_ID:
        return LeafRoute{ endpoint: endpoint, reply: dispatch_lease }
    case service_topology.COMPLETION_MAILBOX_ENDPOINT_ID:
        return LeafRoute{ endpoint: endpoint, reply: dispatch_completion_mailbox }
    case service_topology.OBJECT_STORE_ENDPOINT_ID:
        return LeafRoute{ endpoint: endpoint, reply: dispatch_object_store }
    default:
        return LeafRoute{ endpoint: endpoint, reply: dispatch_invalid_endpoint }
    }
}

// kv dispatch with advisory log marker.
// The kv write always returns its own status; the advisory log append is
// best-effort.  send_dropped is set to 1 when the advisory was not delivered.
func kernel_dispatch_kv(state: *boot.KernelBootState, msg: service_effect.Message) service_effect.Effect {
    current: boot.KernelBootState = *state
    kv_result: kv_service.KvResult = kv_service.handle(current.kv.state, msg)
    new_log_state: log_service.LogServiceState = current.log.state
    advisory_dropped: u32 = 0
    if msg.payload_len >= 2 {
        log_payload: [4]u8 = primitives.zero_payload()
        log_payload[0] = KV_WRITE_LOG_MARKER
        log_msg: service_effect.Message = service_effect.message(msg.source_pid, service_topology.LOG_ENDPOINT_ID, 1, log_payload)
        kv_log_result: log_service.LogResult = log_service.handle(current.log.state, log_msg)
        new_log_state = kv_log_result.state
        if service_effect.effect_reply_status(kv_log_result.effect) == syscall.SyscallStatus.Exhausted {
            advisory_dropped = 1
        }
    }
    *state = boot.bootwith_kv(boot.bootwith_log(current, new_log_state), kv_result.state)
    if advisory_dropped == 1 {
        return service_effect.effect_mark_send_dropped(kv_result.effect)
    }
    return kv_result.effect
}

// Leaf service dispatch: log, kv, echo, and the default for unknown endpoints.
// Shell Send effects target either a leaf service or one bounded shell-owned
// lifecycle control step on the shell endpoint; no leaf service produces a
// further Send that needs chasing through a non-leaf path.
func kernel_dispatch_shell_control(state: *boot.KernelBootState, msg: service_effect.Message) service_effect.Effect {
    invalid_kind: u8 = shell_service.lifecycle_invalid_kind(msg)
    if invalid_kind != 0 {
        return shell_service.invalid_effect(invalid_kind)
    }

    control: shell_service.LifecycleControl = shell_service.lifecycle_control(msg)
    route: LifecycleRoute = lifecycle_route(control.op)
    return route.reply(state, control.target)
}

func kernel_dispatch_leaf(state: *boot.KernelBootState, msg: service_effect.Message) service_effect.Effect {
    route: LeafRoute = leaf_route(msg.endpoint_id)
    return route.reply(state, msg)
}

func kernel_dispatch_transfer_receive(state: *boot.KernelBootState, observation: syscall.ReceiveObservation) service_effect.Effect {
    current: boot.KernelBootState = *state
    grant: transfer_grant.GrantConsume = transfer_grant.grant_consume(current.grants, observation.source_pid, observation.received_handle_slot, observation.received_handle_count)
    if grant.status != syscall.SyscallStatus.Ok {
        return service_effect.effect_reply(grant.status, 0, primitives.zero_payload())
    }
    current = boot.bootwith_grants(current, grant.table)
    *state = current
    transfer_effect: service_effect.Effect = kernel_dispatch_leaf(state, service_effect.message_with_handle(observation.source_pid, observation.endpoint_id, observation.payload_len, observation.payload, observation.received_handle_slot, observation.received_handle_count, grant.endpoint0, grant.endpoint1))
    return execute_effect(state, transfer_effect)
}

func execute_effect(state: *boot.KernelBootState, effect: service_effect.Effect) service_effect.Effect {
    current: service_effect.Effect = effect
    for depth in 0..MAX_EFFECT_CHAIN_DEPTH {
        if service_effect.effect_has_send(current) == 0 {
            return current
        }
        current = kernel_dispatch_leaf(state, effect_to_message(current))
    }
    return service_effect.effect_reply(syscall.SyscallStatus.Exhausted, 0, primitives.zero_payload())
}

// Build the base serial reply effect from the committed path state.
func serial_build_reply(next_path: serial_shell_path.SerialShellPathState) service_effect.Effect {
    return service_effect.effect_reply(serial_shell_path.path_serial_reply_status(next_path), serial_shell_path.path_serial_reply_len(next_path), serial_shell_path.path_serial_reply_payload(next_path))
}

// Attach event markers to a serial reply effect and propagate the
// send_dropped witness from the resolved inner effect.
func serial_attach_events(serial_effect: service_effect.Effect, obs: syscall.ReceiveObservation, next_path: serial_shell_path.SerialShellPathState, resolved: service_effect.Effect) service_effect.Effect {
    result: service_effect.Effect = serial_effect
    if obs.payload_len != 0 {
        if obs.payload[0] == 255 {
            result = service_effect.effect_with_event(result, event_codes.EVENT_SERIAL_REJECTED)
        } else {
            result = service_effect.effect_with_event(result, event_codes.EVENT_SERIAL_BUFFERED)
        }
    }
    if serial_shell_path.path_serial_reply_status(next_path) != syscall.SyscallStatus.None {
        result = service_effect.effect_with_event(result, event_codes.EVENT_SHELL_FORWARDED)
        if serial_shell_path.path_serial_reply_status(next_path) == syscall.SyscallStatus.Ok {
            result = service_effect.effect_with_event(result, event_codes.EVENT_SHELL_REPLY_OK)
        }
        if serial_shell_path.path_serial_reply_status(next_path) == syscall.SyscallStatus.InvalidArgument {
            result = service_effect.effect_with_event(result, event_codes.EVENT_SHELL_REPLY_INVALID)
        }
        if serial_shell_path.path_serial_buffer_len(next_path) == 0 {
            result = service_effect.effect_with_event(result, event_codes.EVENT_SERIAL_CLEARED)
        }
    }
    if service_effect.effect_send_dropped(resolved) != 0 {
        return service_effect.effect_mark_send_dropped(result)
    }
    return result
}

// Serial path dispatch: run path_step, chase the shell→leaf chain, commit
// the resolved reply back into path state, then compose the serial reply.
func kernel_dispatch_serial(state: *boot.KernelBootState, obs: syscall.ReceiveObservation) service_effect.Effect {
    current: boot.KernelBootState = *state
    next_path: serial_shell_path.SerialShellPathState = current.path_state
    inner: service_effect.Effect = serial_shell_path.path_step(&next_path, obs)
    resolved: service_effect.Effect = execute_effect(state, inner)
    next_path = serial_shell_path.path_commit_reply(next_path, resolved)
    current = *state
    *state = boot.bootwith_path(current, next_path)
    return serial_attach_events(serial_build_reply(next_path), obs, next_path, resolved)
}

// Top-level dispatch step: route to the serial path or directly to leaf
// services.  The endpoint_is_boot_wired guard is the explicit address
// check: only known public names route forward; unknown ids return
// InvalidEndpoint here rather than falling through the per-service chain.
// Handle-bearing observations are rejected first: capability metadata is not
// part of ordinary named traffic in the current system.  Knowing a valid
// boot-wired endpoint id is sufficient to address the service only when the
// observation carries no explicit handle metadata.
func kernel_dispatch_step(state: *boot.KernelBootState, observation: syscall.ReceiveObservation) service_effect.Effect {
    if syscall.observation_has_received_handle(observation) {
        switch observation.endpoint_id {
        case service_topology.TRANSFER_ENDPOINT_ID:
            return kernel_dispatch_transfer_receive(state, observation)
        default:
            return service_effect.effect_reply(syscall.SyscallStatus.InvalidCapability, 0, primitives.zero_payload())
        }
    }
    if !service_topology.endpoint_is_boot_wired(observation.endpoint_id) {
        return service_effect.effect_reply(syscall.SyscallStatus.InvalidEndpoint, 0, primitives.zero_payload())
    }
    switch observation.endpoint_id {
    case service_topology.SERIAL_ENDPOINT_ID:
        return kernel_dispatch_serial(state, observation)
    case service_topology.TRANSFER_ENDPOINT_ID:
        return execute_effect(state, kernel_dispatch_leaf(state, service_effect.message(observation.source_pid, observation.endpoint_id, observation.payload_len, observation.payload)))
    default:
        return kernel_dispatch_leaf(state, service_effect.message(observation.source_pid, observation.endpoint_id, observation.payload_len, observation.payload))
    }
}
