import echo_service
import ipc
import kv_service
import log_service
import serial_service
import shell_service

func build_phase145_service_event_observation(source_pid: u32, endpoint_id: u32, event_byte: u8) syscall.ReceiveObservation {
    payload: [4]u8 = ipc.zero_payload()
    payload[0] = event_byte
    return syscall.ReceiveObservation{ status: syscall.SyscallStatus.Ok, block_reason: syscall.BlockReason.None, endpoint_id: endpoint_id, source_pid: source_pid, payload_len: 1, received_handle_slot: 0, received_handle_count: 0, payload: payload }
}

func execute_phase145_service_restart_shell_command(serial_state: serial_service.SerialServiceState, shell_state: shell_service.ShellServiceState, log_state: log_service.LogServiceState, echo_state: echo_service.EchoServiceState, kv_state: kv_service.KvServiceState, shell_endpoint_id: u32, request_len: usize, request_payload: [4]u8) Phase142ShellCommandRouteResult {
    routed: Phase142ShellCommandRouteResult = execute_phase144_stateful_key_value_shell_command(serial_state, shell_state, log_state, echo_state, kv_state, shell_endpoint_id, request_len, request_payload)
    next_log: log_service.LogServiceState = routed.log_state
    next_kv: kv_service.KvServiceState = routed.kv_state
    if kv_service.should_append_failure_log(next_kv) {
        next_log = log_service.record_append_request(next_log, build_phase145_service_event_observation(next_kv.owner_pid, routed.shell_state.log_endpoint_id, kv_service.failure_log_byte()))
        next_log = log_service.record_ack(next_log, log_service.ack_payload()[0])
        next_kv = kv_service.confirm_failure_log(next_kv)
    }
    return Phase142ShellCommandRouteResult{ serial_state: routed.serial_state, shell_state: routed.shell_state, log_state: next_log, echo_state: routed.echo_state, kv_state: next_kv, routing: routed.routing, succeeded: routed.succeeded }
}

func append_phase145_restart_observation(log_state: log_service.LogServiceState, source_pid: u32, log_endpoint_id: u32) log_service.LogServiceState {
    next_log: log_service.LogServiceState = log_service.record_append_request(log_state, build_phase145_service_event_observation(source_pid, log_endpoint_id, kv_service.restart_log_byte()))
    return log_service.record_ack(next_log, log_service.ack_payload()[0])
}