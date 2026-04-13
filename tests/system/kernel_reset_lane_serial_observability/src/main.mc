import ipc
import serial_service
import serial_shell_event_log
import serial_shell_path
import shell_service
import syscall

const EXPECT_SERIAL_BUFFERED: u32 = 1
const EXPECT_SERIAL_REJECTED: u32 = 2
const EXPECT_SHELL_FORWARDED: u32 = 3
const EXPECT_SHELL_REPLY_OK: u32 = 4
const EXPECT_SERIAL_CLEARED: u32 = 6

func build_receive_observation(source_pid: u32, endpoint_id: u32, payload_len: usize, payload: [4]u8) syscall.ReceiveObservation {
    return syscall.ReceiveObservation{ status: syscall.SyscallStatus.Ok, block_reason: syscall.BlockReason.None, endpoint_id: endpoint_id, source_pid: source_pid, payload_len: payload_len, received_handle_slot: 0, received_handle_count: 0, payload: payload }
}

func build_path_state() serial_shell_path.SerialShellPathState {
    return serial_shell_path.service_state(serial_service.serial_init(10, 1), shell_service.shell_init(11, 1), 77)
}

func smoke_valid_step_emits_flat_events() bool {
    payload: [4]u8 = ipc.zero_payload()
    traced: serial_shell_event_log.TracedSerialShellStep

    payload[0] = 72
    payload[1] = 73
    traced = serial_shell_event_log.trace_step(build_path_state(), serial_shell_event_log.event_log_init(), build_receive_observation(9, 55, 2, payload))
    if traced.result.reply_status != syscall.SyscallStatus.Ok {
        return false
    }
    if serial_shell_event_log.event_log_len(traced.event_log) != 4 {
        return false
    }
    if serial_shell_event_log.event_log_entry(traced.event_log, 0) != EXPECT_SERIAL_BUFFERED {
        return false
    }
    if serial_shell_event_log.event_log_entry(traced.event_log, 1) != EXPECT_SHELL_FORWARDED {
        return false
    }
    if serial_shell_event_log.event_log_entry(traced.event_log, 2) != EXPECT_SHELL_REPLY_OK {
        return false
    }
    if serial_shell_event_log.event_log_entry(traced.event_log, 3) != EXPECT_SERIAL_CLEARED {
        return false
    }
    return true
}

func smoke_invalid_byte_emits_reject_event() bool {
    payload: [4]u8 = ipc.zero_payload()
    traced: serial_shell_event_log.TracedSerialShellStep

    payload[0] = 255
    traced = serial_shell_event_log.trace_step(build_path_state(), serial_shell_event_log.event_log_init(), build_receive_observation(9, 55, 1, payload))
    if traced.result.forwarded != 0 {
        return false
    }
    if serial_shell_event_log.event_log_len(traced.event_log) != 1 {
        return false
    }
    if serial_shell_event_log.event_log_entry(traced.event_log, 0) != EXPECT_SERIAL_REJECTED {
        return false
    }
    return true
}

func smoke_ring_keeps_latest_events() bool {
    payload: [4]u8 = ipc.zero_payload()
    event_log: serial_shell_event_log.SerialShellEventLog = serial_shell_event_log.event_log_init()
    traced: serial_shell_event_log.TracedSerialShellStep

    payload[0] = 255
    traced = serial_shell_event_log.trace_step(build_path_state(), event_log, build_receive_observation(9, 55, 1, payload))
    event_log = traced.event_log

    payload = ipc.zero_payload()
    payload[0] = 65
    traced = serial_shell_event_log.trace_step(build_path_state(), event_log, build_receive_observation(9, 55, 1, payload))
    event_log = traced.event_log

    if serial_shell_event_log.event_log_len(event_log) != 4 {
        return false
    }
    if serial_shell_event_log.event_log_entry(event_log, 0) != EXPECT_SERIAL_BUFFERED {
        return false
    }
    if serial_shell_event_log.event_log_entry(event_log, 1) != EXPECT_SHELL_FORWARDED {
        return false
    }
    if serial_shell_event_log.event_log_entry(event_log, 2) != EXPECT_SHELL_REPLY_OK {
        return false
    }
    if serial_shell_event_log.event_log_entry(event_log, 3) != EXPECT_SERIAL_CLEARED {
        return false
    }
    return true
}

func main() i32 {
    if !smoke_valid_step_emits_flat_events() {
        return 1
    }
    if !smoke_invalid_byte_emits_reject_event() {
        return 1
    }
    if !smoke_ring_keeps_latest_events() {
        return 1
    }
    return 0
}