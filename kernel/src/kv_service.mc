import ipc

enum KvMessageTag {
    None,
    Set,
    Get,
}

struct KvServiceState {
    owner_pid: u32
    endpoint_handle_slot: u32
    last_tag: KvMessageTag
    handled_request_count: usize
    key_count: usize
    value_count: usize
    last_client_pid: u32
    last_endpoint_id: u32
    last_key_byte: u8
    last_value_byte: u8
}

struct KvExchangeObservation {
    service_pid: u32
    client_pid: u32
    endpoint_id: u32
    tag: KvMessageTag
    key_byte: u8
    value_byte: u8
    request_count: usize
    key_count: usize
    value_count: usize
}

func service_state(owner_pid: u32, endpoint_handle_slot: u32) KvServiceState {
    return KvServiceState{ owner_pid: owner_pid, endpoint_handle_slot: endpoint_handle_slot, last_tag: KvMessageTag.None, handled_request_count: 0, key_count: 0, value_count: 0, last_client_pid: 0, last_endpoint_id: 0, last_key_byte: 0, last_value_byte: 0 }
}

func record_set(state: KvServiceState, client_pid: u32, endpoint_id: u32, key_byte: u8, value_byte: u8) KvServiceState {
    return KvServiceState{ owner_pid: state.owner_pid, endpoint_handle_slot: state.endpoint_handle_slot, last_tag: KvMessageTag.Set, handled_request_count: state.handled_request_count + 1, key_count: state.key_count + 1, value_count: state.value_count + 1, last_client_pid: client_pid, last_endpoint_id: endpoint_id, last_key_byte: key_byte, last_value_byte: value_byte }
}

func record_get(state: KvServiceState, client_pid: u32, endpoint_id: u32, key_byte: u8) KvServiceState {
    return KvServiceState{ owner_pid: state.owner_pid, endpoint_handle_slot: state.endpoint_handle_slot, last_tag: KvMessageTag.Get, handled_request_count: state.handled_request_count + 1, key_count: state.key_count + 1, value_count: state.value_count, last_client_pid: client_pid, last_endpoint_id: endpoint_id, last_key_byte: key_byte, last_value_byte: state.last_value_byte }
}

func reply_payload(state: KvServiceState) [4]u8 {
    payload: [4]u8 = ipc.zero_payload()
    payload[0] = state.last_key_byte
    payload[1] = state.last_value_byte
    return payload
}

func observe_exchange(state: KvServiceState) KvExchangeObservation {
    return KvExchangeObservation{ service_pid: state.owner_pid, client_pid: state.last_client_pid, endpoint_id: state.last_endpoint_id, tag: state.last_tag, key_byte: state.last_key_byte, value_byte: state.last_value_byte, request_count: state.handled_request_count, key_count: state.key_count, value_count: state.value_count }
}

func tag_score(tag: KvMessageTag) i32 {
    switch tag {
    case KvMessageTag.None:
        return 1
    case KvMessageTag.Set:
        return 2
    case KvMessageTag.Get:
        return 4
    default:
        return 0
    }
    return 0
}