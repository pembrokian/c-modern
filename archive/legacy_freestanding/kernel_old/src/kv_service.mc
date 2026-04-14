import ipc

const RETAINED_KV_CAPACITY: usize = 4
const KV_LOG_INSERT_BYTE: u8 = 83
const KV_LOG_OVERWRITE_BYTE: u8 = 79
const KV_LOG_FAILURE_BYTE: u8 = 70
const KV_LOG_RESTART_BYTE: u8 = 82

enum KvMessageTag {
    None,
    Set,
    Get,
}

struct KvServiceState {
    owner_pid: u32
    endpoint_handle_slot: u32
    available: u32
    last_tag: KvMessageTag
    handled_request_count: usize
    key_count: usize
    value_count: usize
    last_client_pid: u32
    last_endpoint_id: u32
    last_key_byte: u8
    last_value_byte: u8
    set_count: usize
    get_count: usize
    overwrite_count: usize
    missing_count: usize
    unavailable_count: usize
    table_full_count: usize
    retained_count: usize
    last_found: u32
    overwrote_last_request: u32
    missing_last_request: u32
    unavailable_last_request: u32
    table_full_last_request: u32
    pending_log_write: u32
    pending_failure_log: u32
    logged_write_count: usize
    last_logged_byte: u8
    retained_keys: [4]u8
    retained_values: [4]u8
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

struct KvRetentionObservation {
    service_pid: u32
    client_pid: u32
    endpoint_id: u32
    tag: KvMessageTag
    available: u32
    request_count: usize
    key_count: usize
    value_count: usize
    set_count: usize
    get_count: usize
    overwrite_count: usize
    missing_count: usize
    unavailable_count: usize
    table_full_count: usize
    retained_count: usize
    key_byte: u8
    value_byte: u8
    found: u32
    overwrote_last_request: u32
    missing_last_request: u32
    unavailable_last_request: u32
    table_full_last_request: u32
    logged_write_count: usize
    last_logged_byte: u8
    retained_key0: u8
    retained_key1: u8
    retained_key2: u8
    retained_key3: u8
    retained_value0: u8
    retained_value1: u8
    retained_value2: u8
    retained_value3: u8
}

func service_state(owner_pid: u32, endpoint_handle_slot: u32) KvServiceState {
    return KvServiceState{ owner_pid: owner_pid, endpoint_handle_slot: endpoint_handle_slot, available: 1, last_tag: KvMessageTag.None, handled_request_count: 0, key_count: 0, value_count: 0, last_client_pid: 0, last_endpoint_id: 0, last_key_byte: 0, last_value_byte: 0, set_count: 0, get_count: 0, overwrite_count: 0, missing_count: 0, unavailable_count: 0, table_full_count: 0, retained_count: 0, last_found: 0, overwrote_last_request: 0, missing_last_request: 0, unavailable_last_request: 0, table_full_last_request: 0, pending_log_write: 0, pending_failure_log: 0, logged_write_count: 0, last_logged_byte: 0, retained_keys: ipc.zero_payload(), retained_values: ipc.zero_payload() }
}

func record_unavailable(state: KvServiceState, client_pid: u32, endpoint_id: u32, tag: KvMessageTag, key_byte: u8, value_byte: u8) KvServiceState {
    return KvServiceState{ owner_pid: state.owner_pid, endpoint_handle_slot: state.endpoint_handle_slot, available: 0, last_tag: tag, handled_request_count: state.handled_request_count, key_count: state.key_count, value_count: state.value_count, last_client_pid: client_pid, last_endpoint_id: endpoint_id, last_key_byte: key_byte, last_value_byte: value_byte, set_count: state.set_count, get_count: state.get_count, overwrite_count: state.overwrite_count, missing_count: state.missing_count, unavailable_count: state.unavailable_count + 1, table_full_count: state.table_full_count, retained_count: state.retained_count, last_found: 0, overwrote_last_request: 0, missing_last_request: 0, unavailable_last_request: 1, table_full_last_request: 0, pending_log_write: 0, pending_failure_log: 1, logged_write_count: state.logged_write_count, last_logged_byte: state.last_logged_byte, retained_keys: state.retained_keys, retained_values: state.retained_values }
}

func mark_unavailable(state: KvServiceState) KvServiceState {
    return KvServiceState{ owner_pid: state.owner_pid, endpoint_handle_slot: state.endpoint_handle_slot, available: 0, last_tag: state.last_tag, handled_request_count: state.handled_request_count, key_count: state.key_count, value_count: state.value_count, last_client_pid: state.last_client_pid, last_endpoint_id: state.last_endpoint_id, last_key_byte: state.last_key_byte, last_value_byte: state.last_value_byte, set_count: state.set_count, get_count: state.get_count, overwrite_count: state.overwrite_count, missing_count: state.missing_count, unavailable_count: state.unavailable_count, table_full_count: state.table_full_count, retained_count: state.retained_count, last_found: state.last_found, overwrote_last_request: state.overwrote_last_request, missing_last_request: state.missing_last_request, unavailable_last_request: 0, table_full_last_request: state.table_full_last_request, pending_log_write: 0, pending_failure_log: 0, logged_write_count: state.logged_write_count, last_logged_byte: state.last_logged_byte, retained_keys: state.retained_keys, retained_values: state.retained_values }
}

func restart_service(state: KvServiceState) KvServiceState {
    return service_state(state.owner_pid, state.endpoint_handle_slot)
}

func retained_slot_for(keys: [4]u8, retained_count: usize, key_byte: u8) usize {
    if retained_count > 0 && keys[0] == key_byte {
        return 0
    }
    if retained_count > 1 && keys[1] == key_byte {
        return 1
    }
    if retained_count > 2 && keys[2] == key_byte {
        return 2
    }
    if retained_count > 3 && keys[3] == key_byte {
        return 3
    }
    return RETAINED_KV_CAPACITY
}

func write_key_at(keys: [4]u8, slot: usize, key_byte: u8) [4]u8 {
    next_keys: [4]u8 = keys
    if slot == 0 {
        next_keys[0] = key_byte
        return next_keys
    }
    if slot == 1 {
        next_keys[1] = key_byte
        return next_keys
    }
    if slot == 2 {
        next_keys[2] = key_byte
        return next_keys
    }
    if slot == 3 {
        next_keys[3] = key_byte
    }
    return next_keys
}

func write_value_at(values: [4]u8, slot: usize, value_byte: u8) [4]u8 {
    next_values: [4]u8 = values
    if slot == 0 {
        next_values[0] = value_byte
        return next_values
    }
    if slot == 1 {
        next_values[1] = value_byte
        return next_values
    }
    if slot == 2 {
        next_values[2] = value_byte
        return next_values
    }
    if slot == 3 {
        next_values[3] = value_byte
    }
    return next_values
}

func retained_value_at(values: [4]u8, slot: usize) u8 {
    if slot == 0 {
        return values[0]
    }
    if slot == 1 {
        return values[1]
    }
    if slot == 2 {
        return values[2]
    }
    if slot == 3 {
        return values[3]
    }
    return 0
}

func record_set(state: KvServiceState, client_pid: u32, endpoint_id: u32, key_byte: u8, value_byte: u8) KvServiceState {
    if state.available == 0 {
        return record_unavailable(state, client_pid, endpoint_id, KvMessageTag.Set, key_byte, value_byte)
    }
    slot: usize = retained_slot_for(state.retained_keys, state.retained_count, key_byte)
    next_keys: [4]u8 = state.retained_keys
    next_values: [4]u8 = state.retained_values
    retained_count: usize = state.retained_count
    overwrite_count: usize = state.overwrite_count
    table_full_count: usize = state.table_full_count
    overwrote_last_request: u32 = 0
    table_full_last_request: u32 = 0
    pending_log_write: u32 = 1
    last_logged_byte: u8 = KV_LOG_INSERT_BYTE

    if slot != RETAINED_KV_CAPACITY {
        next_values = write_value_at(next_values, slot, value_byte)
        overwrite_count = overwrite_count + 1
        overwrote_last_request = 1
        last_logged_byte = KV_LOG_OVERWRITE_BYTE
    } else {
        if retained_count == RETAINED_KV_CAPACITY {
            table_full_count = table_full_count + 1
            table_full_last_request = 1
            pending_log_write = 0
        } else {
            next_keys = write_key_at(next_keys, retained_count, key_byte)
            next_values = write_value_at(next_values, retained_count, value_byte)
            retained_count = retained_count + 1
        }
    }

    return KvServiceState{ owner_pid: state.owner_pid, endpoint_handle_slot: state.endpoint_handle_slot, available: 1, last_tag: KvMessageTag.Set, handled_request_count: state.handled_request_count + 1, key_count: state.key_count + 1, value_count: state.value_count + 1, last_client_pid: client_pid, last_endpoint_id: endpoint_id, last_key_byte: key_byte, last_value_byte: value_byte, set_count: state.set_count + 1, get_count: state.get_count, overwrite_count: overwrite_count, missing_count: state.missing_count, unavailable_count: state.unavailable_count, table_full_count: table_full_count, retained_count: retained_count, last_found: 1, overwrote_last_request: overwrote_last_request, missing_last_request: 0, unavailable_last_request: 0, table_full_last_request: table_full_last_request, pending_log_write: pending_log_write, pending_failure_log: 0, logged_write_count: state.logged_write_count, last_logged_byte: last_logged_byte, retained_keys: next_keys, retained_values: next_values }
}

func record_get(state: KvServiceState, client_pid: u32, endpoint_id: u32, key_byte: u8) KvServiceState {
    if state.available == 0 {
        return record_unavailable(state, client_pid, endpoint_id, KvMessageTag.Get, key_byte, 0)
    }
    slot: usize = retained_slot_for(state.retained_keys, state.retained_count, key_byte)
    found: u32 = 0
    value_byte: u8 = 0
    missing_count: usize = state.missing_count
    missing_last_request: u32 = 0
    if slot != RETAINED_KV_CAPACITY {
        found = 1
        value_byte = retained_value_at(state.retained_values, slot)
    } else {
        missing_count = missing_count + 1
        missing_last_request = 1
    }
    return KvServiceState{ owner_pid: state.owner_pid, endpoint_handle_slot: state.endpoint_handle_slot, available: 1, last_tag: KvMessageTag.Get, handled_request_count: state.handled_request_count + 1, key_count: state.key_count + 1, value_count: state.value_count, last_client_pid: client_pid, last_endpoint_id: endpoint_id, last_key_byte: key_byte, last_value_byte: value_byte, set_count: state.set_count, get_count: state.get_count + 1, overwrite_count: state.overwrite_count, missing_count: missing_count, unavailable_count: state.unavailable_count, table_full_count: state.table_full_count, retained_count: state.retained_count, last_found: found, overwrote_last_request: 0, missing_last_request: missing_last_request, unavailable_last_request: 0, table_full_last_request: 0, pending_log_write: 0, pending_failure_log: 0, logged_write_count: state.logged_write_count, last_logged_byte: state.last_logged_byte, retained_keys: state.retained_keys, retained_values: state.retained_values }
}

func reply_payload(state: KvServiceState) [4]u8 {
    if state.unavailable_last_request != 0 {
        return unavailable_reply_payload(state)
    }
    payload: [4]u8 = ipc.zero_payload()
    payload[0] = state.last_key_byte
    payload[1] = state.last_value_byte
    if state.last_tag == KvMessageTag.Get {
        if state.last_found != 0 {
            payload[2] = 1
        }
        if state.missing_last_request != 0 {
            payload[3] = 1
        }
        return payload
    }
    if state.overwrote_last_request != 0 {
        payload[2] = 1
    }
    if state.table_full_last_request != 0 {
        payload[3] = 1
    }
    return payload
}

func unavailable_reply_payload(state: KvServiceState) [4]u8 {
    payload: [4]u8 = ipc.zero_payload()
    payload[0] = state.last_key_byte
    payload[1] = state.last_value_byte
    payload[3] = 2
    return payload
}

func observe_exchange(state: KvServiceState) KvExchangeObservation {
    return KvExchangeObservation{ service_pid: state.owner_pid, client_pid: state.last_client_pid, endpoint_id: state.last_endpoint_id, tag: state.last_tag, key_byte: state.last_key_byte, value_byte: state.last_value_byte, request_count: state.handled_request_count, key_count: state.key_count, value_count: state.value_count }
}

func observe_retention(state: KvServiceState) KvRetentionObservation {
    return KvRetentionObservation{ service_pid: state.owner_pid, client_pid: state.last_client_pid, endpoint_id: state.last_endpoint_id, tag: state.last_tag, available: state.available, request_count: state.handled_request_count, key_count: state.key_count, value_count: state.value_count, set_count: state.set_count, get_count: state.get_count, overwrite_count: state.overwrite_count, missing_count: state.missing_count, unavailable_count: state.unavailable_count, table_full_count: state.table_full_count, retained_count: state.retained_count, key_byte: state.last_key_byte, value_byte: state.last_value_byte, found: state.last_found, overwrote_last_request: state.overwrote_last_request, missing_last_request: state.missing_last_request, unavailable_last_request: state.unavailable_last_request, table_full_last_request: state.table_full_last_request, logged_write_count: state.logged_write_count, last_logged_byte: state.last_logged_byte, retained_key0: state.retained_keys[0], retained_key1: state.retained_keys[1], retained_key2: state.retained_keys[2], retained_key3: state.retained_keys[3], retained_value0: state.retained_values[0], retained_value1: state.retained_values[1], retained_value2: state.retained_values[2], retained_value3: state.retained_values[3] }
}

func should_append_log_write(state: KvServiceState) bool {
    return state.pending_log_write != 0
}

func log_write_byte(state: KvServiceState) u8 {
    return state.last_logged_byte
}

func confirm_log_append(state: KvServiceState) KvServiceState {
    return KvServiceState{ owner_pid: state.owner_pid, endpoint_handle_slot: state.endpoint_handle_slot, available: state.available, last_tag: state.last_tag, handled_request_count: state.handled_request_count, key_count: state.key_count, value_count: state.value_count, last_client_pid: state.last_client_pid, last_endpoint_id: state.last_endpoint_id, last_key_byte: state.last_key_byte, last_value_byte: state.last_value_byte, set_count: state.set_count, get_count: state.get_count, overwrite_count: state.overwrite_count, missing_count: state.missing_count, unavailable_count: state.unavailable_count, table_full_count: state.table_full_count, retained_count: state.retained_count, last_found: state.last_found, overwrote_last_request: state.overwrote_last_request, missing_last_request: state.missing_last_request, unavailable_last_request: state.unavailable_last_request, table_full_last_request: state.table_full_last_request, pending_log_write: 0, pending_failure_log: state.pending_failure_log, logged_write_count: state.logged_write_count + 1, last_logged_byte: state.last_logged_byte, retained_keys: state.retained_keys, retained_values: state.retained_values }
}

func should_append_failure_log(state: KvServiceState) bool {
    return state.pending_failure_log != 0
}

func confirm_failure_log(state: KvServiceState) KvServiceState {
    return KvServiceState{ owner_pid: state.owner_pid, endpoint_handle_slot: state.endpoint_handle_slot, available: state.available, last_tag: state.last_tag, handled_request_count: state.handled_request_count, key_count: state.key_count, value_count: state.value_count, last_client_pid: state.last_client_pid, last_endpoint_id: state.last_endpoint_id, last_key_byte: state.last_key_byte, last_value_byte: state.last_value_byte, set_count: state.set_count, get_count: state.get_count, overwrite_count: state.overwrite_count, missing_count: state.missing_count, unavailable_count: state.unavailable_count, table_full_count: state.table_full_count, retained_count: state.retained_count, last_found: state.last_found, overwrote_last_request: state.overwrote_last_request, missing_last_request: state.missing_last_request, unavailable_last_request: state.unavailable_last_request, table_full_last_request: state.table_full_last_request, pending_log_write: state.pending_log_write, pending_failure_log: 0, logged_write_count: state.logged_write_count, last_logged_byte: state.last_logged_byte, retained_keys: state.retained_keys, retained_values: state.retained_values }
}

func failure_log_byte() u8 {
    return KV_LOG_FAILURE_BYTE
}

func restart_log_byte() u8 {
    return KV_LOG_RESTART_BYTE
}

func request_unavailable(state: KvServiceState) bool {
    return state.unavailable_last_request != 0
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