import echo_service
import ipc
import kv_service
import log_service
import serial_service
import shell_service
import syscall

func build_shell_peer_observation(service_pid: u32, endpoint_id: u32, payload_len: usize, payload: [4]u8) syscall.ReceiveObservation {
    return syscall.ReceiveObservation{ status: syscall.SyscallStatus.Ok, block_reason: syscall.BlockReason.None, endpoint_id: endpoint_id, source_pid: service_pid, payload_len: payload_len, received_handle_slot: 0, received_handle_count: 0, payload: payload }
}

func execute_phase142_shell_command(serial_state: serial_service.SerialServiceState, shell_state: shell_service.ShellServiceState, log_state: log_service.LogServiceState, echo_state: echo_service.EchoServiceState, kv_state: kv_service.KvServiceState, shell_endpoint_id: u32, request_len: usize, request_payload: [4]u8) Phase142ShellCommandRouteResult {
    next_serial: serial_service.SerialServiceState = serial_service.record_forward(serial_state, shell_endpoint_id, syscall.SyscallStatus.Ok, request_len, request_payload)
    next_shell: shell_service.ShellServiceState = shell_service.record_request(shell_state, next_serial.owner_pid, shell_endpoint_id, request_len, request_payload)
    next_log: log_service.LogServiceState = log_state
    next_echo: echo_service.EchoServiceState = echo_state
    next_kv: kv_service.KvServiceState = kv_state
    reply_payload: [4]u8 = ipc.zero_payload()

    switch next_shell.last_tag {
    case shell_service.ShellCommandTag.Echo:
        next_echo = echo_service.record_request(next_echo, build_shell_peer_observation(next_shell.owner_pid, shell_service.selected_endpoint_id(next_shell), 2, shell_service.echo_request_payload(next_shell)))
    case shell_service.ShellCommandTag.LogAppend:
        next_log = log_service.record_append_request(next_log, build_shell_peer_observation(next_shell.owner_pid, shell_service.selected_endpoint_id(next_shell), 1, shell_service.log_request_payload(next_shell)))
        next_log = log_service.record_ack(next_log, log_service.ack_payload()[0])
    case shell_service.ShellCommandTag.LogTail:
        next_log = log_service.record_tail_query(next_log, next_shell.owner_pid, shell_service.selected_endpoint_id(next_shell))
    case shell_service.ShellCommandTag.KvSet:
        next_kv = kv_service.record_set(next_kv, next_shell.owner_pid, shell_service.selected_endpoint_id(next_shell), shell_service.kv_key_byte(next_shell), shell_service.kv_value_byte(next_shell))
    case shell_service.ShellCommandTag.KvGet:
        next_kv = kv_service.record_get(next_kv, next_shell.owner_pid, shell_service.selected_endpoint_id(next_shell), shell_service.kv_key_byte(next_shell))
    default:
    }

    reply_payload = shell_service.reply_payload(next_shell, next_echo, next_log, next_kv)
    next_shell = shell_service.record_reply(next_shell, syscall.SyscallStatus.Ok, shell_service.reply_len_for_tag(next_shell.last_tag), reply_payload)
    next_serial = serial_service.record_aggregate_reply(next_serial, build_shell_peer_observation(next_shell.owner_pid, shell_endpoint_id, next_shell.reply_len, reply_payload))
    return Phase142ShellCommandRouteResult{ serial_state: next_serial, shell_state: next_shell, log_state: next_log, echo_state: next_echo, kv_state: next_kv, routing: shell_service.observe_routing(next_shell), succeeded: 1 }
}