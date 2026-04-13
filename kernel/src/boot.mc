import ipc
import kv_service
import log_service
import serial_service
import serial_shell_path
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

struct KernelBootState {
    path_state: serial_shell_path.SerialShellPathState
    log_state: log_service.LogServiceState
    kv_state: kv_service.KvServiceState
}

struct DispatchResult {
    state: KernelBootState
    reply_status: syscall.SyscallStatus
    reply_len: usize
    reply_payload: [4]u8
    routed: u32
}

func kernel_init() KernelBootState {
    return KernelBootState{ path_state: serial_shell_path.service_state(serial_service.serial_init(BOOT_SERIAL_OWNER_PID, 1), shell_service.shell_init(BOOT_SHELL_OWNER_PID, 1), SHELL_ENDPOINT_ID), log_state: log_service.log_init(BOOT_LOG_OWNER_PID, 1), kv_state: kv_service.kv_init(BOOT_KV_OWNER_PID, 1) }
}

func kernel_dispatch_step(state: KernelBootState, observation: syscall.ReceiveObservation) DispatchResult {
    empty_payload: [4]u8 = ipc.zero_payload()
    if observation.endpoint_id == SERIAL_ENDPOINT_ID {
        path_result: serial_shell_path.SerialShellPathResult = serial_shell_path.serial_to_shell_step(state.path_state, observation)
        return DispatchResult{ state: KernelBootState{ path_state: path_result.state, log_state: state.log_state, kv_state: state.kv_state }, reply_status: path_result.reply_status, reply_len: path_result.reply_len, reply_payload: path_result.reply_payload, routed: 1 }
    }
    if observation.endpoint_id == LOG_ENDPOINT_ID {
        next_log: log_service.LogServiceState = log_service.log_on_append(state.log_state, observation)
        return DispatchResult{ state: KernelBootState{ path_state: state.path_state, log_state: next_log, kv_state: state.kv_state }, reply_status: syscall.SyscallStatus.Ok, reply_len: 0, reply_payload: empty_payload, routed: 1 }
    }
    if observation.endpoint_id == KV_ENDPOINT_ID {
        kv_result: kv_service.KvReceiveResult = kv_service.kv_on_receive(state.kv_state, observation)
        return DispatchResult{ state: KernelBootState{ path_state: state.path_state, log_state: state.log_state, kv_state: kv_result.state }, reply_status: kv_result.reply.status, reply_len: kv_result.reply.payload_len, reply_payload: kv_result.reply.payload, routed: 1 }
    }
    return DispatchResult{ state: state, reply_status: syscall.SyscallStatus.InvalidEndpoint, reply_len: 0, reply_payload: empty_payload, routed: 0 }
}

func debug_boot_routed(result: DispatchResult) u32 {
    return result.routed
}
