import serial_service
import shell_service
import syscall

struct SerialShellPathState {
    serial_state: serial_service.SerialServiceState
    shell_state: shell_service.ShellServiceState
    shell_endpoint_id: u32
}

struct SerialShellPathResult {
    state: SerialShellPathState
    reply_status: syscall.SyscallStatus
    reply_len: usize
    reply_payload: [4]u8
    forwarded: u32
}

func service_state(serial_state: serial_service.SerialServiceState, shell_state: shell_service.ShellServiceState, shell_endpoint_id: u32) SerialShellPathState {
    return SerialShellPathState{ serial_state: serial_state, shell_state: shell_state, shell_endpoint_id: shell_endpoint_id }
}

func build_reply_observation(shell_state: shell_service.ShellServiceState, shell_endpoint_id: u32, reply: shell_service.ShellReply) syscall.ReceiveObservation {
    return syscall.ReceiveObservation{ status: reply.status, block_reason: syscall.BlockReason.None, endpoint_id: shell_endpoint_id, source_pid: shell_state.owner_pid, payload_len: reply.payload_len, received_handle_slot: 0, received_handle_count: 0, payload: reply.payload }
}

func serial_to_shell_step(state: SerialShellPathState, observation: syscall.ReceiveObservation) SerialShellPathResult {
    next_serial: serial_service.SerialServiceState = serial_service.serial_on_receive(state.serial_state, observation)
    request_len: usize = serial_service.serial_forward_request_len(next_serial)
    request_payload: [4]u8 = serial_service.serial_forward_request_payload(next_serial)
    if request_len == 0 {
        return SerialShellPathResult{ state: SerialShellPathState{ serial_state: next_serial, shell_state: state.shell_state, shell_endpoint_id: state.shell_endpoint_id }, reply_status: syscall.SyscallStatus.None, reply_len: 0, reply_payload: request_payload, forwarded: 0 }
    }

    shell_reply: shell_service.ShellReply = shell_service.shell_on_request(state.shell_state, request_len, request_payload)
    next_serial = serial_service.serial_on_reply(next_serial, build_reply_observation(state.shell_state, state.shell_endpoint_id, shell_reply))
    return SerialShellPathResult{ state: SerialShellPathState{ serial_state: next_serial, shell_state: state.shell_state, shell_endpoint_id: state.shell_endpoint_id }, reply_status: shell_reply.status, reply_len: shell_reply.payload_len, reply_payload: shell_reply.payload, forwarded: 1 }
}

func debug_step(result: SerialShellPathResult) u32 {
    if result.forwarded == 0 {
        return 0
    }
    return 1
}