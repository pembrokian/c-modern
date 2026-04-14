import ipc
import boot
import kernel_dispatch
import serial_service
import serial_shell_event_log
import service_effect
import syscall

const SERIAL_ENDPOINT_ID: u32 = 10

func build_receive_observation(source_pid: u32, endpoint_id: u32, payload_len: usize, payload: [4]u8) syscall.ReceiveObservation {
    return syscall.ReceiveObservation{ status: syscall.SyscallStatus.Ok, block_reason: syscall.BlockReason.None, endpoint_id: endpoint_id, source_pid: source_pid, payload_len: payload_len, received_handle_slot: 0, received_handle_count: 0, payload: payload }
}

func smoke_valid_step_emits_flat_events() bool {
    payload: [4]u8 = ipc.zero_payload()
    state: boot.KernelBootState = boot.kernel_init()
    event_log: serial_shell_event_log.SerialShellEventLog = serial_shell_event_log.event_log_init()
    effect: service_effect.Effect

    payload[0] = 69
    payload[1] = 67
    payload[2] = 72
    payload[3] = 73
    effect = kernel_dispatch.kernel_dispatch_step(&state, build_receive_observation(9, SERIAL_ENDPOINT_ID, 4, payload))
    event_log = serial_shell_event_log.append_effect_events(event_log, effect)
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if serial_shell_event_log.event_log_len(event_log) != 4 {
        return false
    }
    if serial_shell_event_log.event_log_entry(event_log, 0) != serial_shell_event_log.EVENT_SERIAL_BUFFERED {
        return false
    }
    if serial_shell_event_log.event_log_entry(event_log, 1) != serial_shell_event_log.EVENT_SHELL_FORWARDED {
        return false
    }
    if serial_shell_event_log.event_log_entry(event_log, 2) != serial_shell_event_log.EVENT_SHELL_REPLY_OK {
        return false
    }
    if serial_shell_event_log.event_log_entry(event_log, 3) != serial_shell_event_log.EVENT_SERIAL_CLEARED {
        return false
    }
    return true
}

func smoke_invalid_byte_emits_reject_event() bool {
    payload: [4]u8 = ipc.zero_payload()
    state: boot.KernelBootState = boot.kernel_init()
    event_log: serial_shell_event_log.SerialShellEventLog = serial_shell_event_log.event_log_init()
    effect: service_effect.Effect

    payload[0] = 255
    effect = kernel_dispatch.kernel_dispatch_step(&state, build_receive_observation(9, SERIAL_ENDPOINT_ID, 1, payload))
    event_log = serial_shell_event_log.append_effect_events(event_log, effect)
    if serial_service.serial_forwarded(state.path_state.serial_state) != 0 {
        return false
    }
    if serial_shell_event_log.event_log_len(event_log) != 1 {
        return false
    }
    if serial_shell_event_log.event_log_entry(event_log, 0) != serial_shell_event_log.EVENT_SERIAL_REJECTED {
        return false
    }
    return true
}

func smoke_ring_keeps_latest_events() bool {
    payload: [4]u8 = ipc.zero_payload()
    event_log: serial_shell_event_log.SerialShellEventLog = serial_shell_event_log.event_log_init()
    state: boot.KernelBootState = boot.kernel_init()
    effect: service_effect.Effect

    payload[0] = 255
    effect = kernel_dispatch.kernel_dispatch_step(&state, build_receive_observation(9, SERIAL_ENDPOINT_ID, 1, payload))
    event_log = serial_shell_event_log.append_effect_events(event_log, effect)

    payload = ipc.zero_payload()
    payload[0] = 69
    payload[1] = 67
    payload[2] = 65
    payload[3] = 33
    effect = kernel_dispatch.kernel_dispatch_step(&state, build_receive_observation(9, SERIAL_ENDPOINT_ID, 4, payload))
    event_log = serial_shell_event_log.append_effect_events(event_log, effect)

    if serial_shell_event_log.event_log_len(event_log) != 4 {
        return false
    }
    if serial_shell_event_log.event_log_entry(event_log, 0) != serial_shell_event_log.EVENT_SERIAL_BUFFERED {
        return false
    }
    if serial_shell_event_log.event_log_entry(event_log, 1) != serial_shell_event_log.EVENT_SHELL_FORWARDED {
        return false
    }
    if serial_shell_event_log.event_log_entry(event_log, 2) != serial_shell_event_log.EVENT_SHELL_REPLY_OK {
        return false
    }
    if serial_shell_event_log.event_log_entry(event_log, 3) != serial_shell_event_log.EVENT_SERIAL_CLEARED {
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