import ipc
import syscall

const SHELL_INVALID_REPLY: u8 = 63

struct ShellServiceState {
    owner_pid: u32
    endpoint_handle_slot: u32
}

struct ShellReply {
    status: syscall.SyscallStatus
    payload_len: usize
    payload: [4]u8
}

func shell_init(owner_pid: u32, endpoint_handle_slot: u32) ShellServiceState {
    return ShellServiceState{ owner_pid: owner_pid, endpoint_handle_slot: endpoint_handle_slot }
}

func service_state(owner_pid: u32, endpoint_handle_slot: u32) ShellServiceState {
    return shell_init(owner_pid, endpoint_handle_slot)
}

func shell_on_request(state: ShellServiceState, payload_len: usize, payload: [4]u8) ShellReply {
    reply_payload: [4]u8 = ipc.zero_payload()
    if payload_len == 0 {
        reply_payload[0] = SHELL_INVALID_REPLY
        return ShellReply{ status: syscall.SyscallStatus.InvalidArgument, payload_len: 1, payload: reply_payload }
    }

    reply_payload[0] = payload[0]
    if payload_len > 1 {
        reply_payload[1] = payload[1]
    }
    if payload_len > 2 {
        reply_payload[2] = payload[2]
    }
    if payload_len > 3 {
        reply_payload[3] = payload[3]
    }
    return ShellReply{ status: syscall.SyscallStatus.Ok, payload_len: payload_len, payload: reply_payload }
}

func debug_request(state: ShellServiceState, payload_len: usize) u32 {
    if state.owner_pid == 0 || payload_len == 0 {
        return 0
    }
    return 1
}