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
    if msg.payload_len != 4 {
        return shell_service.invalid_effect(shell_service.SHELL_INVALID_SHAPE)
    }
    if msg.payload[0] != serial_protocol.CMD_X {
        return shell_service.invalid_effect(shell_service.SHELL_INVALID_COMMAND)
    }
    if msg.payload[1] != serial_protocol.CMD_R && msg.payload[1] != serial_protocol.CMD_I {
        return shell_service.invalid_effect(shell_service.SHELL_INVALID_COMMAND)
    }
    if msg.payload[3] != serial_protocol.CMD_BANG {
        return shell_service.invalid_effect(shell_service.SHELL_INVALID_SHAPE)
    }
    if msg.payload[1] == serial_protocol.CMD_I {
        if msg.payload[2] == serial_protocol.TARGET_WORKSET {
            return shell_service.lifecycle_identity_effect(syscall.SyscallStatus.Ok, boot.boot_workset_generation_payload(*state))
        }
        identity_endpoint: u32 = shell_service.lifecycle_target_endpoint(msg.payload[2])
        if identity_endpoint == 0 {
            return shell_service.invalid_effect(shell_service.SHELL_INVALID_COMMAND)
        }
        mark: service_identity.ServiceMark = boot.bootmark_for_endpoint(*state, identity_endpoint)
        return shell_service.lifecycle_identity_effect(syscall.SyscallStatus.Ok, service_identity.mark_generation_payload(mark))
    }
    if msg.payload[2] == serial_protocol.TARGET_AUDIT {
        *state = init.restart_retained_audit_lane(*state)
        return shell_service.lifecycle_effect(syscall.SyscallStatus.Ok, msg.payload[2], serial_protocol.LIFECYCLE_RELOAD)
    }
    if msg.payload[2] == serial_protocol.TARGET_WORKSET {
        *state = init.restart_retained_workset(*state)
        return shell_service.lifecycle_effect(syscall.SyscallStatus.Ok, msg.payload[2], serial_protocol.LIFECYCLE_RELOAD)
    }
    restart_endpoint: u32 = shell_service.lifecycle_target_endpoint(msg.payload[2])
    if restart_endpoint == 0 {
        return shell_service.invalid_effect(shell_service.SHELL_INVALID_COMMAND)
    }
    if !service_topology.service_can_restart(restart_endpoint) {
        return shell_service.lifecycle_effect(syscall.SyscallStatus.InvalidArgument, msg.payload[2], shell_service.lifecycle_mode(restart_endpoint))
    }
    *state = init.restart(*state, restart_endpoint)
    return shell_service.lifecycle_effect(syscall.SyscallStatus.Ok, msg.payload[2], shell_service.lifecycle_mode(restart_endpoint))
}

func kernel_dispatch_leaf(state: *boot.KernelBootState, msg: service_effect.Message) service_effect.Effect {
    current: boot.KernelBootState = *state
    if msg.endpoint_id == service_topology.SHELL_ENDPOINT_ID {
        return kernel_dispatch_shell_control(state, msg)
    }
    if msg.endpoint_id == service_topology.LOG_ENDPOINT_ID {
        log_result: log_service.LogResult = log_service.handle(current.log.state, msg)
        *state = boot.bootwith_log(current, log_result.state)
        return log_result.effect
    }
    if msg.endpoint_id == service_topology.KV_ENDPOINT_ID {
        return kernel_dispatch_kv(state, msg)
    }
    if msg.endpoint_id == service_topology.QUEUE_ENDPOINT_ID {
        queue_result: queue_service.QueueResult = queue_service.handle(current.queue.state, msg)
        *state = boot.bootwith_queue(current, queue_result.state)
        return queue_result.effect
    }
    if msg.endpoint_id == service_topology.ECHO_ENDPOINT_ID {
        echo_result: echo_service.EchoResult = echo_service.handle(current.echo.state, msg)
        *state = boot.bootwith_echo(current, echo_result.state)
        return echo_result.effect
    }
    if msg.endpoint_id == service_topology.TRANSFER_ENDPOINT_ID {
        transfer_result: transfer_service.TransferResult = transfer_service.handle(current.transfer.state, msg)
        *state = boot.bootwith_transfer(current, transfer_result.state)
        return transfer_result.effect
    }
    if msg.endpoint_id == service_topology.TICKET_ENDPOINT_ID {
        ticket_result: ticket_service.TicketResult = ticket_service.handle(current.ticket.state, msg)
        *state = boot.bootwith_ticket(current, ticket_result.state)
        return ticket_result.effect
    }
    return service_effect.effect_reply(syscall.SyscallStatus.InvalidEndpoint, 0, primitives.zero_payload())
}

func kernel_dispatch_transfer_receive(state: *boot.KernelBootState, observation: syscall.ReceiveObservation) service_effect.Effect {
    current: boot.KernelBootState = *state
    grant: transfer_grant.GrantConsume = transfer_grant.grant_consume(current.grants, observation.source_pid, observation.received_handle_slot, observation.received_handle_count)
    if grant.status != syscall.SyscallStatus.Ok {
        return service_effect.effect_reply(grant.status, 0, primitives.zero_payload())
    }
    current = boot.bootwith_grants(current, grant.table)
    *state = current
    transfer_effect: service_effect.Effect = kernel_dispatch_leaf(state, service_effect.message_with_handle(observation.source_pid, observation.endpoint_id, observation.payload_len, observation.payload, observation.received_handle_slot, observation.received_handle_count, grant.endpoint))
    return execute_effect(state, transfer_effect, 0)
}

func execute_effect(state: *boot.KernelBootState, effect: service_effect.Effect, depth: u32) service_effect.Effect {
    if service_effect.effect_has_send(effect) == 0 {
        return effect
    }
    if depth >= MAX_EFFECT_CHAIN_DEPTH {
        return service_effect.effect_reply(syscall.SyscallStatus.Exhausted, 0, primitives.zero_payload())
    }
    next_msg: service_effect.Message = service_effect.message(service_effect.effect_send_source_pid(effect), service_effect.effect_send_endpoint_id(effect), service_effect.effect_send_payload_len(effect), service_effect.effect_send_payload(effect))
    return execute_effect(state, kernel_dispatch_leaf(state, next_msg), depth + 1)
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
    resolved: service_effect.Effect = execute_effect(state, inner, 0)
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
        if observation.endpoint_id == service_topology.TRANSFER_ENDPOINT_ID {
            return kernel_dispatch_transfer_receive(state, observation)
        }
        return service_effect.effect_reply(syscall.SyscallStatus.InvalidCapability, 0, primitives.zero_payload())
    }
    if !service_topology.endpoint_is_boot_wired(observation.endpoint_id) {
        return service_effect.effect_reply(syscall.SyscallStatus.InvalidEndpoint, 0, primitives.zero_payload())
    }
    if observation.endpoint_id == service_topology.SERIAL_ENDPOINT_ID {
        return kernel_dispatch_serial(state, observation)
    }
    return kernel_dispatch_leaf(state, service_effect.message(observation.source_pid, observation.endpoint_id, observation.payload_len, observation.payload))
}
