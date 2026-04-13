import ipc
import log_service
import syscall

func build_receive_observation(source_pid: u32, endpoint_id: u32, payload_len: usize, payload: [4]u8) syscall.ReceiveObservation {
    return syscall.ReceiveObservation{ status: syscall.SyscallStatus.Ok, block_reason: syscall.BlockReason.None, endpoint_id: endpoint_id, source_pid: source_pid, payload_len: payload_len, received_handle_slot: 0, received_handle_count: 0, payload: payload }
}

func smoke_retains_appended_bytes() bool {
    first_payload: [4]u8 = ipc.zero_payload()
    second_payload: [4]u8 = ipc.zero_payload()
    state: log_service.LogServiceState = log_service.log_init(21, 1)
    reply: log_service.LogReply

    first_payload[0] = 65
    second_payload[0] = 66

    state = log_service.log_on_append(state, build_receive_observation(7, 90, 1, first_payload))
    state = log_service.log_on_append(state, build_receive_observation(7, 90, 1, second_payload))
    if log_service.debug_retained_len(state) != 2 {
        return false
    }

    reply = log_service.log_on_tail(state)
    if reply.status != syscall.SyscallStatus.Ok {
        return false
    }
    if reply.payload_len != 2 {
        return false
    }
    if reply.payload[0] != 65 || reply.payload[1] != 66 {
        return false
    }
    return true
}

func smoke_empty_tail_is_empty() bool {
    reply: log_service.LogReply = log_service.log_on_tail(log_service.log_init(21, 1))
    if reply.status != syscall.SyscallStatus.Ok {
        return false
    }
    if reply.payload_len != 0 {
        return false
    }
    return true
}

func main() i32 {
    if !smoke_retains_appended_bytes() {
        return 1
    }
    if !smoke_empty_tail_is_empty() {
        return 1
    }
    return 0
}