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

func message_to_observation(msg: service_effect.Message) syscall.ReceiveObservation {
    return syscall.ReceiveObservation{ status: syscall.SyscallStatus.Ok, block_reason: syscall.BlockReason.None, endpoint_id: msg.endpoint_id, source_pid: msg.source_pid, payload_len: msg.payload_len, received_handle_slot: 0, received_handle_count: 0, payload: msg.payload }
}

func kernel_dispatch_message(state: *KernelBootState, msg: service_effect.Message) service_effect.Effect {
    empty_payload: [4]u8 = primitives.zero_payload()
    current: KernelBootState = *state
    if msg.endpoint_id == SERIAL_ENDPOINT_ID {
        next_path_state: serial_shell_path.SerialShellPathState = current.path_state
        shell_effect: service_effect.Effect = serial_shell_path.path_step(&next_path_state, message_to_observation(msg))
        *state = KernelBootState{ path_state: next_path_state, log_state: current.log_state, kv_state: current.kv_state }
        return shell_effect
    }
    if msg.endpoint_id == LOG_ENDPOINT_ID {
        log_result: log_service.LogResult = log_service.handle(current.log_state, msg)
        *state = KernelBootState{ path_state: current.path_state, log_state: log_result.state, kv_state: current.kv_state }
        return log_result.effect
    }
    if msg.endpoint_id == KV_ENDPOINT_ID {
        kv_result: kv_service.KvResult = kv_service.handle(current.kv_state, msg)
        new_log_state: log_service.LogServiceState = current.log_state
        if msg.payload_len >= 2 {
            log_payload: [4]u8 = primitives.zero_payload()
            log_payload[0] = KV_WRITE_LOG_MARKER
            log_msg: service_effect.Message = service_effect.message(msg.source_pid, LOG_ENDPOINT_ID, 1, log_payload)
            // The kv-write log marker is advisory: if the log is full the
            // Exhausted reply is intentionally discarded here.  The kv reply
            // is what the caller receives; the marker is for observability only.
            kv_log_result: log_service.LogResult = log_service.handle(current.log_state, log_msg)
            new_log_state = kv_log_result.state
        }
        *state = KernelBootState{ path_state: current.path_state, log_state: new_log_state, kv_state: kv_result.state }
        return kv_result.effect
    }
    return service_effect.effect_reply(syscall.SyscallStatus.InvalidEndpoint, 0, empty_payload)
}

func execute_effect(state: *KernelBootState, effect: service_effect.Effect, depth: u32) service_effect.Effect {
    if service_effect.effect_has_send(effect) == 0 {
        return effect
    }
    if depth >= MAX_EFFECT_CHAIN_DEPTH {
        return service_effect.effect_reply(syscall.SyscallStatus.Exhausted, 0, primitives.zero_payload())
    }
    next_msg: service_effect.Message = service_effect.message(service_effect.effect_send_source_pid(effect), service_effect.effect_send_endpoint_id(effect), service_effect.effect_send_payload_len(effect), service_effect.effect_send_payload(effect))
    return execute_effect(state, kernel_dispatch_message(state, next_msg), depth + 1)
}

func serial_path_post_dispatch(state: *KernelBootState, observation: syscall.ReceiveObservation, effect: service_effect.Effect) service_effect.Effect {
    current: KernelBootState = *state
    if serial_shell_path.path_serial_reply_status(current.path_state) == syscall.SyscallStatus.None {
        if service_effect.effect_has_reply(effect) != 0 {
            new_path: serial_shell_path.SerialShellPathState = serial_shell_path.path_receive_reply(current.path_state, effect)
            *state = KernelBootState{ path_state: new_path, log_state: current.log_state, kv_state: current.kv_state }
        }
    }
    current = *state
    serial_effect: service_effect.Effect = service_effect.effect_reply(serial_shell_path.path_serial_reply_status(current.path_state), serial_shell_path.path_serial_reply_len(current.path_state), serial_shell_path.path_serial_reply_payload(current.path_state))
    if observation.payload_len != 0 {
        if observation.payload[0] == 255 {
            serial_effect = service_effect.effect_with_event(serial_effect, EVENT_SERIAL_REJECTED)
        } else {
            serial_effect = service_effect.effect_with_event(serial_effect, EVENT_SERIAL_BUFFERED)
        }
    }
    if serial_shell_path.path_serial_reply_status(current.path_state) != syscall.SyscallStatus.None {
        serial_effect = service_effect.effect_with_event(serial_effect, EVENT_SHELL_FORWARDED)
        if serial_shell_path.path_serial_reply_status(current.path_state) == syscall.SyscallStatus.Ok {
            serial_effect = service_effect.effect_with_event(serial_effect, EVENT_SHELL_REPLY_OK)
        }
        if serial_shell_path.path_serial_reply_status(current.path_state) == syscall.SyscallStatus.InvalidArgument {
            serial_effect = service_effect.effect_with_event(serial_effect, EVENT_SHELL_REPLY_INVALID)
        }
        if serial_shell_path.path_serial_buffer_len(current.path_state) == 0 {
            serial_effect = service_effect.effect_with_event(serial_effect, EVENT_SERIAL_CLEARED)
        }
    }
    return serial_effect
}

func kernel_dispatch_step(state: *KernelBootState, observation: syscall.ReceiveObservation) service_effect.Effect {
    start_msg: service_effect.Message = service_effect.message(observation.source_pid, observation.endpoint_id, observation.payload_len, observation.payload)
    effect: service_effect.Effect = execute_effect(state, kernel_dispatch_message(state, start_msg), 0)
    if observation.endpoint_id == SERIAL_ENDPOINT_ID {
        return serial_path_post_dispatch(state, observation, effect)
    }
    return effect
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
