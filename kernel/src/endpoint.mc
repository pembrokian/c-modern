const ENDPOINT_TABLE_CAPACITY: usize = 2
const ENDPOINT_QUEUE_CAPACITY: usize = 2
const ENDPOINT_NOT_FOUND: usize = 2

enum MessageState {
    Empty,
    Queued,
    Delivered,
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

struct EndpointSlot {
    endpoint_id: u32
    owner_pid: u32
    head: usize
    tail: usize
    queued_messages: usize
    active: u32
    messages: [2]KernelMessage
}

struct EndpointTable {
    count: usize
    slots: [2]EndpointSlot
}

struct EndpointDispatch {
    endpoint_id: u32
    endpoint_index: usize
    valid: u32
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
    return EndpointSlot{ endpoint_id: 0, owner_pid: 0, head: 0, tail: 0, queued_messages: 0, active: 0, messages: zero_messages() }
}

func zero_slots() [2]EndpointSlot {
    slots: [2]EndpointSlot
    slots[0] = empty_slot()
    slots[1] = empty_slot()
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

func install_endpoint(table: EndpointTable, owner_pid: u32, endpoint_id: u32) EndpointTable {
    if table.count >= ENDPOINT_TABLE_CAPACITY {
        return table
    }
    slots: [2]EndpointSlot = table.slots
    slots[table.count] = EndpointSlot{ endpoint_id: endpoint_id, owner_pid: owner_pid, head: 0, tail: 0, queued_messages: 0, active: 1, messages: zero_messages() }
    return EndpointTable{ count: table.count + 1, slots: slots }
}

func find_endpoint_index(table: EndpointTable, endpoint_id: u32) usize {
    if table.count == 0 {
        return ENDPOINT_NOT_FOUND
    }
    if table.slots[0].endpoint_id == endpoint_id && table.slots[0].active == 1 {
        return 0
    }
    if table.count < ENDPOINT_TABLE_CAPACITY {
        return ENDPOINT_NOT_FOUND
    }
    if table.slots[1].endpoint_id == endpoint_id && table.slots[1].active == 1 {
        return 1
    }
    return ENDPOINT_NOT_FOUND
}

func resolve_runtime_endpoint(table: EndpointTable, endpoint_id: u32) EndpointDispatch {
    endpoint_index: usize = find_endpoint_index(table, endpoint_id)
    if !endpoint_index_valid(table, endpoint_index) {
        return EndpointDispatch{ endpoint_id: endpoint_id, endpoint_index: 0, valid: 0 }
    }
    return EndpointDispatch{ endpoint_id: endpoint_id, endpoint_index: endpoint_index, valid: 1 }
}

func build_runtime_message(message_id: u32, source_pid: u32, endpoint_id: u32, payload_len: usize, payload: [4]u8, attached_count: usize, attached_endpoint_id: u32, attached_rights: u32, attached_source_handle_slot: u32) KernelMessage {
    if attached_count == 1 {
        return attached_message(message_id, source_pid, endpoint_id, payload_len, payload, attached_endpoint_id, attached_rights, attached_source_handle_slot)
    }
    return byte_message(message_id, source_pid, endpoint_id, payload_len, payload)
}

func enqueue_runtime_message(table: EndpointTable, endpoint_id: u32, message: KernelMessage) RuntimeSendResult {
    dispatch: EndpointDispatch = resolve_runtime_endpoint(table, endpoint_id)
    if dispatch.valid == 0 {
        return RuntimeSendResult{ endpoints: table, endpoint_id: endpoint_id, queued: 0, queue_full: 0, endpoint_valid: 0 }
    }
    updated_endpoints: EndpointTable = enqueue_message(table, dispatch.endpoint_index, message)
    if !enqueue_succeeded(table, updated_endpoints, dispatch.endpoint_index) {
        return RuntimeSendResult{ endpoints: table, endpoint_id: endpoint_id, queued: 0, queue_full: 1, endpoint_valid: 1 }
    }
    return RuntimeSendResult{ endpoints: updated_endpoints, endpoint_id: endpoint_id, queued: 1, queue_full: 0, endpoint_valid: 1 }
}

func receive_runtime_message(table: EndpointTable, endpoint_id: u32) RuntimeReceiveResult {
    dispatch: EndpointDispatch = resolve_runtime_endpoint(table, endpoint_id)
    if dispatch.valid == 0 {
        return RuntimeReceiveResult{ endpoints: table, endpoint_id: endpoint_id, message: empty_message(), available: 0, queue_empty: 0, endpoint_valid: 0 }
    }
    message: KernelMessage = peek_head_message(table, dispatch.endpoint_index)
    if message_state_score(message.state) == 1 {
        return RuntimeReceiveResult{ endpoints: table, endpoint_id: endpoint_id, message: empty_message(), available: 0, queue_empty: 1, endpoint_valid: 1 }
    }
    updated_endpoints: EndpointTable = consume_head_message(table, dispatch.endpoint_index)
    return RuntimeReceiveResult{ endpoints: updated_endpoints, endpoint_id: endpoint_id, message: message, available: 1, queue_empty: 0, endpoint_valid: 1 }
}

func validate_syscall_ipc_boundary() bool {
    table: EndpointTable = empty_table()
    table = install_endpoint(table, 2, 9)
    payload: [4]u8 = zero_payload()
    payload[0] = 83
    payload[1] = 89
    payload[2] = 83
    payload[3] = 67
    message: KernelMessage = build_runtime_message(5, 2, 9, 4, payload, 0, 0, 0, 0)
    sent: RuntimeSendResult = enqueue_runtime_message(table, 9, message)
    if sent.endpoint_valid == 0 || sent.queued == 0 {
        return false
    }
    received: RuntimeReceiveResult = receive_runtime_message(sent.endpoints, 9)
    if received.endpoint_valid == 0 || received.available == 0 {
        return false
    }
    if received.message.source_pid != 2 {
        return false
    }
    if received.message.payload[0] != 83 {
        return false
    }
    empty_receive: RuntimeReceiveResult = receive_runtime_message(received.endpoints, 9)
    if empty_receive.queue_empty == 0 {
        return false
    }
    return true
}

func endpoint_index_valid(table: EndpointTable, endpoint_index: usize) bool {
    if endpoint_index >= ENDPOINT_TABLE_CAPACITY {
        return false
    }
    return endpoint_index < table.count
}

func enqueue_message(table: EndpointTable, endpoint_index: usize, message: KernelMessage) EndpointTable {
    if !endpoint_index_valid(table, endpoint_index) {
        return table
    }
    slots: [2]EndpointSlot = table.slots
    current: EndpointSlot = slots[endpoint_index]
    if current.active == 0 {
        return table
    }
    if current.queued_messages >= ENDPOINT_QUEUE_CAPACITY {
        return table
    }
    messages: [2]KernelMessage = current.messages
    tail: usize = current.tail
    messages[tail] = message
    tail = advance_index(tail)
    slots[endpoint_index] = EndpointSlot{ endpoint_id: current.endpoint_id, owner_pid: current.owner_pid, head: current.head, tail: tail, queued_messages: current.queued_messages + 1, active: current.active, messages: messages }
    return EndpointTable{ count: table.count, slots: slots }
}

func enqueue_succeeded(before: EndpointTable, after: EndpointTable, endpoint_index: usize) bool {
    if !endpoint_index_valid(before, endpoint_index) {
        return false
    }
    return after.slots[endpoint_index].queued_messages == before.slots[endpoint_index].queued_messages + 1
}

func peek_head_message(table: EndpointTable, endpoint_index: usize) KernelMessage {
    if !endpoint_index_valid(table, endpoint_index) {
        return empty_message()
    }
    current: EndpointSlot = table.slots[endpoint_index]
    if current.queued_messages == 0 {
        return empty_message()
    }
    return current.messages[current.head]
}

func consume_head_message(table: EndpointTable, endpoint_index: usize) EndpointTable {
    if !endpoint_index_valid(table, endpoint_index) {
        return table
    }
    slots: [2]EndpointSlot = table.slots
    current: EndpointSlot = slots[endpoint_index]
    if current.queued_messages == 0 {
        return table
    }
    messages: [2]KernelMessage = current.messages
    head: usize = current.head
    messages[head] = empty_message()
    head = advance_index(head)
    count: usize = current.queued_messages - 1
    tail: usize = current.tail
    if count == 0 {
        head = 0
        tail = 0
    }
    slots[endpoint_index] = EndpointSlot{ endpoint_id: current.endpoint_id, owner_pid: current.owner_pid, head: head, tail: tail, queued_messages: count, active: current.active, messages: messages }
    return EndpointTable{ count: table.count, slots: slots }
}

func message_state_score(state: MessageState) i32 {
    switch state {
    case MessageState.Empty:
        return 1
    case MessageState.Queued:
        return 2
    case MessageState.Delivered:
        return 4
    default:
        return 0
    }
    return 0
}