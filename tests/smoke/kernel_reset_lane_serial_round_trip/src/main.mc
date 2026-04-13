import ipc
import serial_service
import serial_shell_path
import shell_service
import syscall

func build_receive_observation(source_pid: u32, endpoint_id: u32, payload_len: usize, payload: [4]u8) syscall.ReceiveObservation {
    return syscall.ReceiveObservation{ status: syscall.SyscallStatus.Ok, block_reason: syscall.BlockReason.None, endpoint_id: endpoint_id, source_pid: source_pid, payload_len: payload_len, received_handle_slot: 0, received_handle_count: 0, payload: payload }
}

func build_path_state() serial_shell_path.SerialShellPathState {
    return serial_shell_path.service_state(serial_service.serial_init(10, 1), shell_service.shell_init(11, 1), 77)
}

func smoke_round_trip_passes() bool {
    payload: [4]u8 = ipc.zero_payload()
    result: serial_shell_path.SerialShellPathResult

    payload[0] = 72
    payload[1] = 73
    result = serial_shell_path.serial_to_shell_step(build_path_state(), build_receive_observation(9, 55, 2, payload))
    if result.forwarded == 0 {
        return false
    }
    if result.reply_status != syscall.SyscallStatus.Ok {
        return false
    }
    if result.reply_len != 2 {
        return false
    }
    if result.reply_payload[0] != 72 || result.reply_payload[1] != 73 {
        return false
    }
    if result.state.serial_state.len != 0 {
        return false
    }
    return true
}

func smoke_invalid_byte_does_not_forward() bool {
    payload: [4]u8 = ipc.zero_payload()
    result: serial_shell_path.SerialShellPathResult

    payload[0] = 255
    result = serial_shell_path.serial_to_shell_step(build_path_state(), build_receive_observation(9, 55, 1, payload))
    if result.forwarded != 0 {
        return false
    }
    if result.reply_status != syscall.SyscallStatus.None {
        return false
    }
    if result.state.serial_state.len != 0 {
        return false
    }
    return true
}

func main() i32 {
    if !smoke_round_trip_passes() {
        return 1
    }
    if !smoke_invalid_byte_does_not_forward() {
        return 1
    }
    return 0
}