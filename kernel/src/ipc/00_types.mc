const ENDPOINT_TABLE_CAPACITY: usize = 4
const ENDPOINT_QUEUE_CAPACITY: usize = 2
const ENDPOINT_NOT_FOUND: usize = 4

enum MessageState {
    Empty,
    Queued,
    Delivered,
    Aborted,
}

struct KernelMessage {
    message_id: u32
    source_pid: u32
    endpoint_id: u32
    len: usize
    attached_count: usize
    attached_endpoint_id: u32
    attached_rights: u32
    attached_source_handle_slot: u32
    state: MessageState
    payload: [4]u8
}

enum EndpointWakeReason {
    None,
    QueueSpaceAvailable,
    MessageAvailable,
    EndpointClosed,
}

struct EndpointWaiter {
    task_slot: u32
    task_id: u32
    waiting: u32
}

struct EndpointWake {
    task_slot: u32
    task_id: u32
    wake_reason: EndpointWakeReason
    woke: u32
}

struct EndpointWakeList {
    count: usize
    wakes: [4]EndpointWake
}

struct EndpointSlot {
    endpoint_id: u32
    owner_pid: u32
    head: usize
    tail: usize
    queued_messages: usize
    active: u32
    blocked_sender: EndpointWaiter
    blocked_receiver: EndpointWaiter
    messages: [2]KernelMessage
}

struct EndpointTable {
    count: usize
    slots: [4]EndpointSlot
}

struct EndpointDispatch {
    endpoint_id: u32
    endpoint_index: usize
    valid: u32
}

struct EndpointLookup {
    endpoint_id: u32
    endpoint_index: usize
    found: u32
    active: u32
}

struct RuntimeSendResult {
    endpoints: EndpointTable
    endpoint_id: u32
    queued: u32
    queue_full: u32
    endpoint_valid: u32
}

struct RuntimeReceiveResult {
    endpoints: EndpointTable
    endpoint_id: u32
    message: KernelMessage
    available: u32
    queue_empty: u32
    endpoint_valid: u32
}

struct RuntimeSendTransition {
    endpoints: EndpointTable
    endpoint_id: u32
    wake: EndpointWake
    queued: u32
    queue_full: u32
    endpoint_valid: u32
    endpoint_closed: u32
}

struct RuntimeReceiveTransition {
    endpoints: EndpointTable
    endpoint_id: u32
    message: KernelMessage
    wake: EndpointWake
    available: u32
    queue_empty: u32
    endpoint_valid: u32
    endpoint_closed: u32
}

struct EndpointWakeTransition {
    endpoints: EndpointTable
    wake: EndpointWake
}

struct AbortMessagesResult {
    messages: [2]KernelMessage
    aborted_messages: usize
}

struct EndpointCloseSlotTransition {
    slot: EndpointSlot
    wakes: EndpointWakeList
    aborted_messages: usize
    closed: u32
}

struct EndpointCloseTransition {
    endpoints: EndpointTable
    endpoint_id: u32
    wakes: EndpointWakeList
    aborted_messages: usize
    endpoint_found: u32
    closed: u32
}

struct EndpointOwnerCloseTransition {
    endpoints: EndpointTable
    wakes: EndpointWakeList
    aborted_messages: usize
    closed_count: usize
}

func zero_payload() [4]u8 {
    payload: [4]u8
    payload[0] = 0
    payload[1] = 0
    payload[2] = 0
    payload[3] = 0
    return payload
}

func empty_message() KernelMessage {
    return KernelMessage{ message_id: 0, source_pid: 0, endpoint_id: 0, len: 0, attached_count: 0, attached_endpoint_id: 0, attached_rights: 0, attached_source_handle_slot: 0, state: MessageState.Empty, payload: zero_payload() }
}

func empty_waiter() EndpointWaiter {
    return EndpointWaiter{ task_slot: 0, task_id: 0, waiting: 0 }
}

func no_wake() EndpointWake {
    return EndpointWake{ task_slot: 0, task_id: 0, wake_reason: EndpointWakeReason.None, woke: 0 }
}

func zero_wakes() [4]EndpointWake {
    wakes: [4]EndpointWake
    wakes[0] = no_wake()
    wakes[1] = no_wake()
    wakes[2] = no_wake()
    wakes[3] = no_wake()
    return wakes
}

func empty_wake_list() EndpointWakeList {
    return EndpointWakeList{ count: 0, wakes: zero_wakes() }
}

func append_wake(list: EndpointWakeList, wake: EndpointWake) EndpointWakeList {
    if wake.woke == 0 {
        return list
    }
    if list.count >= 4 {
        return list
    }
    wakes: [4]EndpointWake = list.wakes
    wakes[list.count] = wake
    return EndpointWakeList{ count: list.count + 1, wakes: wakes }
}

func bootstrap_init_message(source_pid: u32, endpoint_id: u32) KernelMessage {
    payload: [4]u8 = zero_payload()
    payload[0] = 73
    payload[1] = 78
    payload[2] = 73
    payload[3] = 84
    return KernelMessage{ message_id: 1, source_pid: source_pid, endpoint_id: endpoint_id, len: 4, attached_count: 0, attached_endpoint_id: 0, attached_rights: 0, attached_source_handle_slot: 0, state: MessageState.Queued, payload: payload }
}

func mark_delivered(message: KernelMessage) KernelMessage {
    return KernelMessage{ message_id: message.message_id, source_pid: message.source_pid, endpoint_id: message.endpoint_id, len: message.len, attached_count: message.attached_count, attached_endpoint_id: message.attached_endpoint_id, attached_rights: message.attached_rights, attached_source_handle_slot: message.attached_source_handle_slot, state: MessageState.Delivered, payload: message.payload }
}

func byte_message(message_id: u32, source_pid: u32, endpoint_id: u32, payload_len: usize, payload: [4]u8) KernelMessage {
    return KernelMessage{ message_id: message_id, source_pid: source_pid, endpoint_id: endpoint_id, len: payload_len, attached_count: 0, attached_endpoint_id: 0, attached_rights: 0, attached_source_handle_slot: 0, state: MessageState.Queued, payload: payload }
}

func attached_message(message_id: u32, source_pid: u32, endpoint_id: u32, payload_len: usize, payload: [4]u8, attached_endpoint_id: u32, attached_rights: u32, attached_source_handle_slot: u32) KernelMessage {
    return KernelMessage{ message_id: message_id, source_pid: source_pid, endpoint_id: endpoint_id, len: payload_len, attached_count: 1, attached_endpoint_id: attached_endpoint_id, attached_rights: attached_rights, attached_source_handle_slot: attached_source_handle_slot, state: MessageState.Queued, payload: payload }
}

func zero_messages() [2]KernelMessage {
    messages: [2]KernelMessage
    messages[0] = empty_message()
    messages[1] = empty_message()
    return messages
}

func empty_slot() EndpointSlot {
    return EndpointSlot{ endpoint_id: 0, owner_pid: 0, head: 0, tail: 0, queued_messages: 0, active: 0, blocked_sender: empty_waiter(), blocked_receiver: empty_waiter(), messages: zero_messages() }
}

func zero_slots() [4]EndpointSlot {
    slots: [4]EndpointSlot
    slots[0] = empty_slot()
    slots[1] = empty_slot()
    slots[2] = empty_slot()
    slots[3] = empty_slot()
    return slots
}

func empty_table() EndpointTable {
    return EndpointTable{ count: 0, slots: zero_slots() }
}

func advance_index(index: usize) usize {
    next: usize = index + 1
    if next == ENDPOINT_QUEUE_CAPACITY {
        return 0
    }
    return next
}

func find_endpoint_slot(table: EndpointTable, endpoint_id: u32) usize {
    if table.count == 0 {
        return ENDPOINT_NOT_FOUND
    }
    if table.slots[0].endpoint_id == endpoint_id {
        return 0
    }
    if table.count > 1 && table.slots[1].endpoint_id == endpoint_id {
        return 1
    }
    if table.count > 2 && table.slots[2].endpoint_id == endpoint_id {
        return 2
    }
    if table.count > 3 && table.slots[3].endpoint_id == endpoint_id {
        return 3
    }
    return ENDPOINT_NOT_FOUND
}

func install_endpoint(table: EndpointTable, owner_pid: u32, endpoint_id: u32) EndpointTable {
    existing_index: usize = find_endpoint_slot(table, endpoint_id)
    if existing_index != ENDPOINT_NOT_FOUND {
        current: EndpointSlot = table.slots[existing_index]
        if current.active == 1 {
            return table
        }
        reused_slots: [4]EndpointSlot = table.slots
        reused_slots[existing_index] = EndpointSlot{ endpoint_id: endpoint_id, owner_pid: owner_pid, head: 0, tail: 0, queued_messages: 0, active: 1, blocked_sender: empty_waiter(), blocked_receiver: empty_waiter(), messages: zero_messages() }
        return EndpointTable{ count: table.count, slots: reused_slots }
    }
    if table.count >= ENDPOINT_TABLE_CAPACITY {
        return table
    }
    appended_slots: [4]EndpointSlot = table.slots
    appended_slots[table.count] = EndpointSlot{ endpoint_id: endpoint_id, owner_pid: owner_pid, head: 0, tail: 0, queued_messages: 0, active: 1, blocked_sender: empty_waiter(), blocked_receiver: empty_waiter(), messages: zero_messages() }
    return EndpointTable{ count: table.count + 1, slots: appended_slots }
}

func wake_reason_score(reason: EndpointWakeReason) i32 {
    switch reason {
    case EndpointWakeReason.None:
        return 1
    case EndpointWakeReason.QueueSpaceAvailable:
        return 2
    case EndpointWakeReason.MessageAvailable:
        return 4
    case EndpointWakeReason.EndpointClosed:
        return 8
    default:
        return 0
    }
    return 0
}

func find_endpoint_index(table: EndpointTable, endpoint_id: u32) usize {
    if table.count == 0 {
        return ENDPOINT_NOT_FOUND
    }
    if table.slots[0].endpoint_id == endpoint_id && table.slots[0].active == 1 {
        return 0
    }
    if table.count > 1 && table.slots[1].endpoint_id == endpoint_id && table.slots[1].active == 1 {
        return 1
    }
    if table.count > 2 && table.slots[2].endpoint_id == endpoint_id && table.slots[2].active == 1 {
        return 2
    }
    if table.count > 3 && table.slots[3].endpoint_id == endpoint_id && table.slots[3].active == 1 {
        return 3
    }
    return ENDPOINT_NOT_FOUND
}

func lookup_runtime_endpoint(table: EndpointTable, endpoint_id: u32) EndpointLookup {
    endpoint_index: usize = find_endpoint_slot(table, endpoint_id)
    if endpoint_index == ENDPOINT_NOT_FOUND {
        return EndpointLookup{ endpoint_id: endpoint_id, endpoint_index: 0, found: 0, active: 0 }
    }
    active: u32 = table.slots[endpoint_index].active
    return EndpointLookup{ endpoint_id: endpoint_id, endpoint_index: endpoint_index, found: 1, active: active }
}

func resolve_runtime_endpoint(table: EndpointTable, endpoint_id: u32) EndpointDispatch {
    endpoint_index: usize = find_endpoint_index(table, endpoint_id)
    if !endpoint_index_valid(table, endpoint_index) {
        return EndpointDispatch{ endpoint_id: endpoint_id, endpoint_index: 0, valid: 0 }
    }
    return EndpointDispatch{ endpoint_id: endpoint_id, endpoint_index: endpoint_index, valid: 1 }
}

func message_state_score(state: MessageState) i32 {
    switch state {
    case MessageState.Empty:
        return 1
    case MessageState.Queued:
        return 2
    case MessageState.Delivered:
        return 4
    case MessageState.Aborted:
        return 8
    default:
        return 0
    }
    return 0
}