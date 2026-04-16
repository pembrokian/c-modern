// Dispatch and routing layer for the kernel boot state.
//
// This module owns all runtime dispatch logic: leaf service routing,
// the effect-chasing loop, and the serial path sequence.
//
// boot.mc owns configuration, state construction, and service refs.
// This module owns how an incoming observation is turned into an Effect.

import boot
import echo_service
import init
import event_codes
import kv_service
import log_service
import primitives
import queue_service
import serial_protocol
import serial_shell_path
import service_effect
import service_identity
import service_topology
import shell_service
import syscall
import ticket_service
import transfer_grant
import transfer_service

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
    return target == serial_protocol.TARGET_WORKSET || target == serial_protocol.TARGET_AUDIT
}

func lifecycle_op_supported(op: u8) bool {
    return op == serial_protocol.CMD_I || op == serial_protocol.CMD_S || op == serial_protocol.CMD_R
}

func lifecycle_validate(msg: service_effect.Message) u8 {
    if msg.payload_len != 4 {
        return shell_service.SHELL_INVALID_SHAPE
    }
    if msg.payload[0] != serial_protocol.CMD_X {
        return shell_service.SHELL_INVALID_COMMAND
    }
    if !lifecycle_op_supported(msg.payload[1]) {
        return shell_service.SHELL_INVALID_COMMAND
    }
    if msg.payload[3] != serial_protocol.CMD_BANG {
        return shell_service.SHELL_INVALID_SHAPE
    }
    return 0
}

func lifecycle_target_endpoint(target: u8) u32 {
    return shell_service.lifecycle_target_endpoint(target)
}

func lifecycle_identity_reply(state: *boot.KernelBootState, target: u8) service_effect.Effect {
    if target == serial_protocol.TARGET_WORKSET {
        return shell_service.lifecycle_identity_effect(syscall.SyscallStatus.Ok, boot.boot_workset_generation_payload(*state))
    }
    identity_endpoint: u32 = lifecycle_target_endpoint(target)
    if identity_endpoint == 0 {
        return shell_service.invalid_effect(shell_service.SHELL_INVALID_COMMAND)
    }
    mark: service_identity.ServiceMark = boot.bootmark_for_endpoint(*state, identity_endpoint)
    return shell_service.lifecycle_identity_effect(syscall.SyscallStatus.Ok, service_identity.mark_generation_payload(mark))
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

func lifecycle_invalid_reply(state: *boot.KernelBootState, target: u8) service_effect.Effect {
    return shell_service.invalid_effect(shell_service.SHELL_INVALID_COMMAND)
}

func lifecycle_route(op: u8) LifecycleRoute {
    switch op {
    case serial_protocol.CMD_I:
        return LifecycleRoute{ op: op, reply: lifecycle_identity_reply }
    case serial_protocol.CMD_S:
        return LifecycleRoute{ op: op, reply: lifecycle_summary_reply }
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
    invalid_kind: u8 = lifecycle_validate(msg)
    if invalid_kind != 0 {
        return shell_service.invalid_effect(invalid_kind)
    }

    route: LifecycleRoute = lifecycle_route(msg.payload[1])
    return route.reply(state, msg.payload[2])
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
