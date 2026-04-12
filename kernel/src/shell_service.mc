import echo_service
import ipc
import kv_service
import log_service
import syscall

enum ShellCommandTag {
    None,
    Echo,
    LogAppend,
    LogTail,
    KvSet,
    KvGet,
    Invalid,
}

struct ShellServiceState {
    owner_pid: u32
    endpoint_handle_slot: u32
    last_tag: ShellCommandTag
    handled_command_count: usize
    last_client_pid: u32
    last_endpoint_id: u32
    last_request_len: usize
    last_byte0: u8
    last_byte1: u8
    last_byte2: u8
    last_byte3: u8
    echo_route_count: usize
    log_route_count: usize
    kv_route_count: usize
    invalid_command_count: usize
    echo_endpoint_id: u32
    log_endpoint_id: u32
    kv_endpoint_id: u32
    reply_status: syscall.SyscallStatus
    reply_len: usize
    reply_byte0: u8
    reply_byte1: u8
    reply_byte2: u8
    reply_byte3: u8
}

struct ShellRoutingObservation {
    service_pid: u32
    client_pid: u32
    endpoint_id: u32
    tag: ShellCommandTag
    request_len: usize
    request_byte0: u8
    request_byte1: u8
    request_byte2: u8
    request_byte3: u8
    echo_endpoint_id: u32
    log_endpoint_id: u32
    kv_endpoint_id: u32
    echo_route_count: usize
    log_route_count: usize
    kv_route_count: usize
    invalid_command_count: usize
    reply_status: syscall.SyscallStatus
    reply_len: usize
    reply_byte0: u8
    reply_byte1: u8
    reply_byte2: u8
    reply_byte3: u8
}

func service_state(owner_pid: u32, endpoint_handle_slot: u32, echo_endpoint_id: u32, log_endpoint_id: u32, kv_endpoint_id: u32) ShellServiceState {
    return ShellServiceState{ owner_pid: owner_pid, endpoint_handle_slot: endpoint_handle_slot, last_tag: ShellCommandTag.None, handled_command_count: 0, last_client_pid: 0, last_endpoint_id: 0, last_request_len: 0, last_byte0: 0, last_byte1: 0, last_byte2: 0, last_byte3: 0, echo_route_count: 0, log_route_count: 0, kv_route_count: 0, invalid_command_count: 0, echo_endpoint_id: echo_endpoint_id, log_endpoint_id: log_endpoint_id, kv_endpoint_id: kv_endpoint_id, reply_status: syscall.SyscallStatus.None, reply_len: 0, reply_byte0: 0, reply_byte1: 0, reply_byte2: 0, reply_byte3: 0 }
}

func classify_command(payload_len: usize, payload: [4]u8) ShellCommandTag {
    if payload_len < 2 {
        return ShellCommandTag.Invalid
    }
    if payload[0] == 69 && payload[1] == 67 {
        return ShellCommandTag.Echo
    }
    if payload[0] == 76 {
        if payload[1] == 65 && payload_len >= 3 {
            return ShellCommandTag.LogAppend
        }
        if payload[1] == 84 {
            return ShellCommandTag.LogTail
        }
    }
    if payload[0] == 75 {
        if payload[1] == 83 && payload_len == 4 {
            return ShellCommandTag.KvSet
        }
        if payload[1] == 71 && payload_len >= 3 {
            return ShellCommandTag.KvGet
        }
    }
    return ShellCommandTag.Invalid
}

func record_request(state: ShellServiceState, source_pid: u32, endpoint_id: u32, payload_len: usize, payload: [4]u8) ShellServiceState {
    byte0: u8 = 0
    byte1: u8 = 0
    byte2: u8 = 0
    byte3: u8 = 0
    if payload_len != 0 {
        byte0 = payload[0]
    }
    if payload_len > 1 {
        byte1 = payload[1]
    }
    if payload_len > 2 {
        byte2 = payload[2]
    }
    if payload_len > 3 {
        byte3 = payload[3]
    }
    tag: ShellCommandTag = classify_command(payload_len, payload)
    echo_route_count: usize = state.echo_route_count
    log_route_count: usize = state.log_route_count
    kv_route_count: usize = state.kv_route_count
    invalid_command_count: usize = state.invalid_command_count
    switch tag {
    case ShellCommandTag.Echo:
        echo_route_count = echo_route_count + 1
    case ShellCommandTag.LogAppend:
        log_route_count = log_route_count + 1
    case ShellCommandTag.LogTail:
        log_route_count = log_route_count + 1
    case ShellCommandTag.KvSet:
        kv_route_count = kv_route_count + 1
    case ShellCommandTag.KvGet:
        kv_route_count = kv_route_count + 1
    case ShellCommandTag.Invalid:
        invalid_command_count = invalid_command_count + 1
    default:
        invalid_command_count = invalid_command_count + 1
    }
    return ShellServiceState{ owner_pid: state.owner_pid, endpoint_handle_slot: state.endpoint_handle_slot, last_tag: tag, handled_command_count: state.handled_command_count + 1, last_client_pid: source_pid, last_endpoint_id: endpoint_id, last_request_len: payload_len, last_byte0: byte0, last_byte1: byte1, last_byte2: byte2, last_byte3: byte3, echo_route_count: echo_route_count, log_route_count: log_route_count, kv_route_count: kv_route_count, invalid_command_count: invalid_command_count, echo_endpoint_id: state.echo_endpoint_id, log_endpoint_id: state.log_endpoint_id, kv_endpoint_id: state.kv_endpoint_id, reply_status: state.reply_status, reply_len: state.reply_len, reply_byte0: state.reply_byte0, reply_byte1: state.reply_byte1, reply_byte2: state.reply_byte2, reply_byte3: state.reply_byte3 }
}

func record_reply(state: ShellServiceState, status: syscall.SyscallStatus, payload_len: usize, payload: [4]u8) ShellServiceState {
    return ShellServiceState{ owner_pid: state.owner_pid, endpoint_handle_slot: state.endpoint_handle_slot, last_tag: state.last_tag, handled_command_count: state.handled_command_count, last_client_pid: state.last_client_pid, last_endpoint_id: state.last_endpoint_id, last_request_len: state.last_request_len, last_byte0: state.last_byte0, last_byte1: state.last_byte1, last_byte2: state.last_byte2, last_byte3: state.last_byte3, echo_route_count: state.echo_route_count, log_route_count: state.log_route_count, kv_route_count: state.kv_route_count, invalid_command_count: state.invalid_command_count, echo_endpoint_id: state.echo_endpoint_id, log_endpoint_id: state.log_endpoint_id, kv_endpoint_id: state.kv_endpoint_id, reply_status: status, reply_len: payload_len, reply_byte0: payload[0], reply_byte1: payload[1], reply_byte2: payload[2], reply_byte3: payload[3] }
}

func reply_payload(state: ShellServiceState, echo_state: echo_service.EchoServiceState, log_state: log_service.LogServiceState, kv_state: kv_service.KvServiceState) [4]u8 {
    payload: [4]u8 = ipc.zero_payload()
    switch state.last_tag {
    case ShellCommandTag.Echo:
        payload = echo_service.reply_payload(echo_state)
        return payload
    case ShellCommandTag.LogAppend:
        payload = log_service.reply_payload(log_state)
        return payload
    case ShellCommandTag.LogTail:
        payload = log_service.reply_payload(log_state)
        return payload
    case ShellCommandTag.KvSet:
        payload = kv_service.reply_payload(kv_state)
        return payload
    case ShellCommandTag.KvGet:
        payload = kv_service.reply_payload(kv_state)
        return payload
    default:
        payload[0] = 63
        return payload
    }
    return payload
}

func selected_endpoint_id(state: ShellServiceState) u32 {
    switch state.last_tag {
    case ShellCommandTag.Echo:
        return state.echo_endpoint_id
    case ShellCommandTag.LogAppend:
        return state.log_endpoint_id
    case ShellCommandTag.LogTail:
        return state.log_endpoint_id
    case ShellCommandTag.KvSet:
        return state.kv_endpoint_id
    case ShellCommandTag.KvGet:
        return state.kv_endpoint_id
    default:
        return 0
    }
    return 0
}

func reply_len_for_tag(tag: ShellCommandTag) usize {
    switch tag {
    case ShellCommandTag.KvSet:
        return 4
    case ShellCommandTag.KvGet:
        return 4
    case ShellCommandTag.Invalid:
        return 1
    default:
        return 2
    }
    return 1
}

func echo_request_payload(state: ShellServiceState) [4]u8 {
    payload: [4]u8 = ipc.zero_payload()
    payload[0] = state.last_byte2
    payload[1] = state.last_byte3
    return payload
}

func log_request_payload(state: ShellServiceState) [4]u8 {
    payload: [4]u8 = ipc.zero_payload()
    payload[0] = state.last_byte2
    return payload
}

func kv_key_byte(state: ShellServiceState) u8 {
    return state.last_byte2
}

func kv_value_byte(state: ShellServiceState) u8 {
    return state.last_byte3
}

func observe_routing(state: ShellServiceState) ShellRoutingObservation {
    return ShellRoutingObservation{ service_pid: state.owner_pid, client_pid: state.last_client_pid, endpoint_id: state.last_endpoint_id, tag: state.last_tag, request_len: state.last_request_len, request_byte0: state.last_byte0, request_byte1: state.last_byte1, request_byte2: state.last_byte2, request_byte3: state.last_byte3, echo_endpoint_id: state.echo_endpoint_id, log_endpoint_id: state.log_endpoint_id, kv_endpoint_id: state.kv_endpoint_id, echo_route_count: state.echo_route_count, log_route_count: state.log_route_count, kv_route_count: state.kv_route_count, invalid_command_count: state.invalid_command_count, reply_status: state.reply_status, reply_len: state.reply_len, reply_byte0: state.reply_byte0, reply_byte1: state.reply_byte1, reply_byte2: state.reply_byte2, reply_byte3: state.reply_byte3 }
}

func tag_score(tag: ShellCommandTag) i32 {
    switch tag {
    case ShellCommandTag.None:
        return 1
    case ShellCommandTag.Echo:
        return 2
    case ShellCommandTag.LogAppend:
        return 4
    case ShellCommandTag.LogTail:
        return 8
    case ShellCommandTag.KvSet:
        return 16
    case ShellCommandTag.KvGet:
        return 32
    case ShellCommandTag.Invalid:
        return 64
    default:
        return 0
    }
    return 0
}