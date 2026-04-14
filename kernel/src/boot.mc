import primitives
import kv_service
import log_service
import serial_service
import serial_shell_path
import service_effect
import service_identity
import shell_service
import syscall

const SERIAL_ENDPOINT_ID: u32 = 10
const SHELL_ENDPOINT_ID: u32 = 11
const LOG_ENDPOINT_ID: u32 = 12
const KV_ENDPOINT_ID: u32 = 13

const BOOT_SERIAL_OWNER_PID: u32 = 1
const BOOT_SHELL_OWNER_PID: u32 = 2
const BOOT_LOG_OWNER_PID: u32 = 3
const BOOT_KV_OWNER_PID: u32 = 4
// MAX_EFFECT_CHAIN_DEPTH guards against send loops between services.
// If the chain exceeds this depth the original caller receives Exhausted.
// Value 8 is well above the current topology (serial → shell → kv/log, depth ≤ 3).
const MAX_EFFECT_CHAIN_DEPTH: u32 = 8
const KV_WRITE_LOG_MARKER: u8 = 75
const EVENT_SERIAL_BUFFERED: u32 = 1
const EVENT_SERIAL_REJECTED: u32 = 2
const EVENT_SHELL_FORWARDED: u32 = 3
const EVENT_SHELL_REPLY_OK: u32 = 4
const EVENT_SHELL_REPLY_INVALID: u32 = 5
const EVENT_SERIAL_CLEARED: u32 = 6

struct KernelBootState {
    path_state: serial_shell_path.SerialShellPathState
    log_state: log_service.LogServiceState
    kv_state: kv_service.KvServiceState
}

func kernel_init() KernelBootState {
    return KernelBootState{ path_state: serial_shell_path.path_init(serial_service.serial_init(BOOT_SERIAL_OWNER_PID, 1), shell_service.shell_init(BOOT_SHELL_OWNER_PID, 1, LOG_ENDPOINT_ID, KV_ENDPOINT_ID), SHELL_ENDPOINT_ID), log_state: log_service.log_init(BOOT_LOG_OWNER_PID, 1), kv_state: kv_service.kv_init(BOOT_KV_OWNER_PID, 1) }
}

// kv dispatch with advisory log marker.
// The kv write always returns its own status; the advisory log append is
// best-effort.  send_dropped is set to 1 when the advisory was not delivered.
func kernel_dispatch_kv(state: *KernelBootState, msg: service_effect.Message) service_effect.Effect {
    current: KernelBootState = *state
    kv_result: kv_service.KvResult = kv_service.handle(current.kv_state, msg)
    new_log_state: log_service.LogServiceState = current.log_state
    advisory_dropped: u32 = 0
    if msg.payload_len >= 2 {
        log_payload: [4]u8 = primitives.zero_payload()
        log_payload[0] = KV_WRITE_LOG_MARKER
        log_msg: service_effect.Message = service_effect.message(msg.source_pid, LOG_ENDPOINT_ID, 1, log_payload)
        kv_log_result: log_service.LogResult = log_service.handle(current.log_state, log_msg)
        new_log_state = kv_log_result.state
        if service_effect.effect_reply_status(kv_log_result.effect) == syscall.SyscallStatus.Exhausted {
            advisory_dropped = 1
        }
    }
    *state = KernelBootState{ path_state: current.path_state, log_state: new_log_state, kv_state: kv_result.state }
    if advisory_dropped == 1 {
        return service_effect.effect_mark_send_dropped(kv_result.effect)
    }
    return kv_result.effect
}

// Leaf service dispatch: log, kv, and the default for unknown endpoints.
// Shell Send effects always target log or kv; no leaf service produces
// a further Send that needs chasing through a non-leaf path.
func kernel_dispatch_leaf(state: *KernelBootState, msg: service_effect.Message) service_effect.Effect {
    current: KernelBootState = *state
    if msg.endpoint_id == LOG_ENDPOINT_ID {
        log_result: log_service.LogResult = log_service.handle(current.log_state, msg)
        *state = KernelBootState{ path_state: current.path_state, log_state: log_result.state, kv_state: current.kv_state }
        return log_result.effect
    }
    if msg.endpoint_id == KV_ENDPOINT_ID {
        return kernel_dispatch_kv(state, msg)
    }
    return service_effect.effect_reply(syscall.SyscallStatus.InvalidEndpoint, 0, primitives.zero_payload())
}

func execute_effect(state: *KernelBootState, effect: service_effect.Effect, depth: u32) service_effect.Effect {
    if service_effect.effect_has_send(effect) == 0 {
        return effect
    }
    if depth >= MAX_EFFECT_CHAIN_DEPTH {
        return service_effect.effect_reply(syscall.SyscallStatus.Exhausted, 0, primitives.zero_payload())
    }
    next_msg: service_effect.Message = service_effect.message(service_effect.effect_send_source_pid(effect), service_effect.effect_send_endpoint_id(effect), service_effect.effect_send_payload_len(effect), service_effect.effect_send_payload(effect))
    return execute_effect(state, kernel_dispatch_leaf(state, next_msg), depth + 1)
}

// Serial path dispatch: run path_step, chase the shell→leaf chain, commit
// the resolved reply back into path state, then assemble the serial reply
// with event markers and the send_dropped witness.
func kernel_dispatch_serial(state: *KernelBootState, obs: syscall.ReceiveObservation) service_effect.Effect {
    current: KernelBootState = *state
    next_path: serial_shell_path.SerialShellPathState = current.path_state
    inner: service_effect.Effect = serial_shell_path.path_step(&next_path, obs)
    resolved: service_effect.Effect = execute_effect(state, inner, 0)
    next_path = serial_shell_path.path_commit_reply(next_path, resolved)
    current = *state
    *state = KernelBootState{ path_state: next_path, log_state: current.log_state, kv_state: current.kv_state }
    serial_effect: service_effect.Effect = service_effect.effect_reply(serial_shell_path.path_serial_reply_status(next_path), serial_shell_path.path_serial_reply_len(next_path), serial_shell_path.path_serial_reply_payload(next_path))
    if obs.payload_len != 0 {
        if obs.payload[0] == 255 {
            serial_effect = service_effect.effect_with_event(serial_effect, EVENT_SERIAL_REJECTED)
        } else {
            serial_effect = service_effect.effect_with_event(serial_effect, EVENT_SERIAL_BUFFERED)
        }
    }
    if serial_shell_path.path_serial_reply_status(next_path) != syscall.SyscallStatus.None {
        serial_effect = service_effect.effect_with_event(serial_effect, EVENT_SHELL_FORWARDED)
        if serial_shell_path.path_serial_reply_status(next_path) == syscall.SyscallStatus.Ok {
            serial_effect = service_effect.effect_with_event(serial_effect, EVENT_SHELL_REPLY_OK)
        }
        if serial_shell_path.path_serial_reply_status(next_path) == syscall.SyscallStatus.InvalidArgument {
            serial_effect = service_effect.effect_with_event(serial_effect, EVENT_SHELL_REPLY_INVALID)
        }
        if serial_shell_path.path_serial_buffer_len(next_path) == 0 {
            serial_effect = service_effect.effect_with_event(serial_effect, EVENT_SERIAL_CLEARED)
        }
    }
    if service_effect.effect_send_dropped(resolved) != 0 {
        return service_effect.effect_mark_send_dropped(serial_effect)
    }
    return serial_effect
}

// Top-level dispatch step: route to the serial path or directly to leaf
// services.  No endpoint-specific post-processing needed at this level.
func kernel_dispatch_step(state: *KernelBootState, observation: syscall.ReceiveObservation) service_effect.Effect {
    if observation.endpoint_id == SERIAL_ENDPOINT_ID {
        return kernel_dispatch_serial(state, observation)
    }
    return kernel_dispatch_leaf(state, service_effect.message(observation.source_pid, observation.endpoint_id, observation.payload_len, observation.payload))
}

func debug_boot_routed(effect: service_effect.Effect) u32 {
    if service_effect.effect_reply_status(effect) == syscall.SyscallStatus.InvalidEndpoint {
        return 0
    }
    return 1
}

// Named ServiceRef accessors for the four boot-wired services.
// These refs are stable across restart — the endpoint_id never changes after
// kernel_init() assigns it.  Callers that hold one of these refs may resume
// sending after a service restart without reacquiring the ref.

func boot_serial_ref() service_identity.ServiceRef {
    return service_identity.service_ref(SERIAL_ENDPOINT_ID)
}

func boot_shell_ref() service_identity.ServiceRef {
    return service_identity.service_ref(SHELL_ENDPOINT_ID)
}

func boot_log_ref() service_identity.ServiceRef {
    return service_identity.service_ref(LOG_ENDPOINT_ID)
}

func boot_kv_ref() service_identity.ServiceRef {
    return service_identity.service_ref(KV_ENDPOINT_ID)
}
