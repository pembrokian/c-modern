import serial_service
import service_effect
import shell_service
import syscall

struct SerialShellPathState {
    serial_state: serial_service.SerialServiceState
    shell_state: shell_service.ShellServiceState
    shell_endpoint_id: u32
}

func path_init(serial_state: serial_service.SerialServiceState, shell_state: shell_service.ShellServiceState, shell_endpoint_id: u32) SerialShellPathState {
    return SerialShellPathState{ serial_state: serial_state, shell_state: shell_state, shell_endpoint_id: shell_endpoint_id }
}

func pathwith(p: SerialShellPathState, serial: serial_service.SerialServiceState, shell: shell_service.ShellServiceState) SerialShellPathState {
    return SerialShellPathState{ serial_state: serial, shell_state: shell, shell_endpoint_id: p.shell_endpoint_id }
}

func build_reply_observation(shell_state: shell_service.ShellServiceState, shell_endpoint_id: u32, effect: service_effect.Effect) syscall.ReceiveObservation {
    return syscall.ReceiveObservation{ status: service_effect.effect_reply_status(effect), block_reason: syscall.BlockReason.None, endpoint_id: shell_endpoint_id, source_pid: shell_state.pid, payload_len: service_effect.effect_reply_payload_len(effect), received_handle_slot: 0, received_handle_count: 0, payload: service_effect.effect_reply_payload(effect) }
}

func serial_shell_step(serial: *serial_service.SerialServiceState, shell: *shell_service.ShellServiceState, shell_endpoint_id: u32, obs: syscall.ReceiveObservation) service_effect.Effect {
    current_serial: serial_service.SerialServiceState = *serial
    current_shell: shell_service.ShellServiceState = *shell

    *serial = serial_service.serial_on_receive(current_serial, obs)
    if serial_service.serial_forward_request_len(*serial) == 0 {
        return service_effect.effect_none()
    }

    shell_effect: service_effect.Effect = shell_service.handle(current_shell, service_effect.message(current_shell.pid, shell_endpoint_id, serial_service.serial_forward_request_len(*serial), serial_service.serial_forward_request_payload(*serial)))
    if service_effect.effect_has_reply(shell_effect) != 0 {
        *serial = serial_service.serial_on_reply(*serial, build_reply_observation(current_shell, shell_endpoint_id, shell_effect))
    }

    return shell_effect
}

func path_step(path: *SerialShellPathState, obs: syscall.ReceiveObservation) service_effect.Effect {
    current: SerialShellPathState = *path
    serial_state: serial_service.SerialServiceState = current.serial_state
    shell_state: shell_service.ShellServiceState = current.shell_state
    effect: service_effect.Effect = serial_shell_step(&serial_state, &shell_state, current.shell_endpoint_id, obs)
    *path = pathwith(current, serial_state, shell_state)
    return effect
}

func path_serial_reply_status(path: SerialShellPathState) syscall.SyscallStatus {
    return serial_service.serial_reply_status(path.serial_state)
}

func path_serial_reply_len(path: SerialShellPathState) usize {
    return serial_service.serial_reply_len(path.serial_state)
}

func path_serial_reply_payload(path: SerialShellPathState) [4]u8 {
    return serial_service.serial_reply_payload(path.serial_state)
}

func path_serial_buffer_len(path: SerialShellPathState) usize {
    return serial_service.serial_forward_request_len(path.serial_state)
}

func path_receive_reply(path: SerialShellPathState, effect: service_effect.Effect) SerialShellPathState {
    obs: syscall.ReceiveObservation = syscall.ReceiveObservation{ status: service_effect.effect_reply_status(effect), block_reason: syscall.BlockReason.None, endpoint_id: path.shell_endpoint_id, source_pid: path.shell_state.pid, payload_len: service_effect.effect_reply_payload_len(effect), received_handle_slot: 0, received_handle_count: 0, payload: service_effect.effect_reply_payload(effect) }
    new_serial: serial_service.SerialServiceState = serial_service.serial_on_reply(path.serial_state, obs)
    return pathwith(path, new_serial, path.shell_state)
}

// Commit an inner resolved effect into the path's serial reply slot.
// If the serial path already has a reply, or the resolved effect is not
// a reply, the path is returned unchanged.
func path_commit_reply(path: SerialShellPathState, inner_effect: service_effect.Effect) SerialShellPathState {
    if path_serial_reply_status(path) != syscall.SyscallStatus.None {
        return path
    }
    if service_effect.effect_has_reply(inner_effect) == 0 {
        return path
    }
    return path_receive_reply(path, inner_effect)
}