import ipc
import kv_service
import syscall

func build_receive_observation(source_pid: u32, endpoint_id: u32, payload_len: usize, payload: [4]u8) syscall.ReceiveObservation {
    return syscall.ReceiveObservation{ status: syscall.SyscallStatus.Ok, block_reason: syscall.BlockReason.None, endpoint_id: endpoint_id, source_pid: source_pid, payload_len: payload_len, received_handle_slot: 0, received_handle_count: 0, payload: payload }
}

func smoke_set_and_get_returns_value() bool {
    state: kv_service.KvServiceState = kv_service.kv_init(21, 1)
    payload: [4]u8 = ipc.zero_payload()
    set_result: kv_service.KvReceiveResult
    get_result: kv_service.KvReceiveResult

    payload[0] = 1
    payload[1] = 42
    set_result = kv_service.kv_on_receive(state, build_receive_observation(7, 90, 2, payload))
    if set_result.reply.status != syscall.SyscallStatus.Ok {
        return false
    }

    payload[1] = 0
    get_result = kv_service.kv_on_receive(set_result.state, build_receive_observation(7, 90, 1, payload))
    if get_result.reply.status != syscall.SyscallStatus.Ok {
        return false
    }
    if get_result.reply.payload_len != 2 {
        return false
    }
    if get_result.reply.payload[1] != 42 {
        return false
    }
    return true
}

func smoke_get_missing_key_fails() bool {
    state: kv_service.KvServiceState = kv_service.kv_init(21, 1)
    payload: [4]u8 = ipc.zero_payload()
    result: kv_service.KvReceiveResult

    payload[0] = 99
    result = kv_service.kv_on_receive(state, build_receive_observation(7, 90, 1, payload))
    if result.reply.status != syscall.SyscallStatus.InvalidArgument {
        return false
    }
    return true
}

func smoke_overwrite_updates_value() bool {
    state: kv_service.KvServiceState = kv_service.kv_init(21, 1)
    payload: [4]u8 = ipc.zero_payload()
    result: kv_service.KvReceiveResult

    payload[0] = 3
    payload[1] = 10
    result = kv_service.kv_on_receive(state, build_receive_observation(7, 90, 2, payload))

    payload[1] = 99
    result = kv_service.kv_on_receive(result.state, build_receive_observation(7, 90, 2, payload))

    result = kv_service.kv_on_receive(result.state, build_receive_observation(7, 90, 1, payload))
    if result.reply.status != syscall.SyscallStatus.Ok {
        return false
    }
    if result.reply.payload[1] != 99 {
        return false
    }
    return true
}

func smoke_distinct_keys_are_independent() bool {
    state: kv_service.KvServiceState = kv_service.kv_init(21, 1)
    payload: [4]u8 = ipc.zero_payload()
    result: kv_service.KvReceiveResult

    payload[0] = 1
    payload[1] = 10
    result = kv_service.kv_on_receive(state, build_receive_observation(7, 90, 2, payload))

    payload[0] = 2
    payload[1] = 20
    result = kv_service.kv_on_receive(result.state, build_receive_observation(7, 90, 2, payload))

    payload[0] = 1
    result = kv_service.kv_on_receive(result.state, build_receive_observation(7, 90, 1, payload))
    if result.reply.payload[1] != 10 {
        return false
    }

    payload[0] = 2
    result = kv_service.kv_on_receive(result.state, build_receive_observation(7, 90, 1, payload))
    if result.reply.payload[1] != 20 {
        return false
    }
    return true
}

func main() i32 {
    if !smoke_set_and_get_returns_value() {
        return 1
    }
    if !smoke_get_missing_key_fails() {
        return 1
    }
    if !smoke_overwrite_updates_value() {
        return 1
    }
    if !smoke_distinct_keys_are_independent() {
        return 1
    }
    return 0
}
