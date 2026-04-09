enum MessageState {
    Empty,
    Queued,
    Delivered,
}

struct KernelMessage {
    message_id: u32
    source_pid: u32
    endpoint_id: u32
    handle_slot: u32
    len: usize
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

func zero_payload() [4]u8 {
    payload: [4]u8
    payload[0] = 0
    payload[1] = 0
    payload[2] = 0
    payload[3] = 0
    return payload
}

func empty_message() KernelMessage {
    return KernelMessage{ message_id: 0, source_pid: 0, endpoint_id: 0, handle_slot: 0, len: 0, state: MessageState.Empty, payload: zero_payload() }
}

func bootstrap_init_message(source_pid: u32, endpoint_id: u32, handle_slot: u32) KernelMessage {
    payload: [4]u8 = zero_payload()
    payload[0] = 73
    payload[1] = 78
    payload[2] = 73
    payload[3] = 84
    return KernelMessage{ message_id: 1, source_pid: source_pid, endpoint_id: endpoint_id, handle_slot: handle_slot, len: 4, state: MessageState.Queued, payload: payload }
}

func mark_delivered(message: KernelMessage) KernelMessage {
    return KernelMessage{ message_id: message.message_id, source_pid: message.source_pid, endpoint_id: message.endpoint_id, handle_slot: message.handle_slot, len: message.len, state: MessageState.Delivered, payload: message.payload }
}

func byte_message(message_id: u32, source_pid: u32, endpoint_id: u32, payload_len: usize, payload: [4]u8) KernelMessage {
    return KernelMessage{ message_id: message_id, source_pid: source_pid, endpoint_id: endpoint_id, handle_slot: 0, len: payload_len, state: MessageState.Queued, payload: payload }
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
    if next == 2 {
        return 0
    }
    return next
}

func install_endpoint(table: EndpointTable, owner_pid: u32, endpoint_id: u32) EndpointTable {
    if table.count >= 2 {
        return table
    }
    slots: [2]EndpointSlot = table.slots
    slots[table.count] = EndpointSlot{ endpoint_id: endpoint_id, owner_pid: owner_pid, head: 0, tail: 0, queued_messages: 0, active: 1, messages: zero_messages() }
    return EndpointTable{ count: table.count + 1, slots: slots }
}

func find_endpoint_index(table: EndpointTable, endpoint_id: u32) usize {
    if table.count == 0 {
        return table.count
    }
    if table.slots[0].endpoint_id == endpoint_id && table.slots[0].active == 1 {
        return 0
    }
    if table.count < 2 {
        return table.count
    }
    if table.slots[1].endpoint_id == endpoint_id && table.slots[1].active == 1 {
        return 1
    }
    return table.count
}

func enqueue_message(table: EndpointTable, endpoint_index: usize, message: KernelMessage) EndpointTable {
    if endpoint_index >= table.count {
        return table
    }
    slots: [2]EndpointSlot = table.slots
    current: EndpointSlot = slots[endpoint_index]
    if current.active == 0 {
        return table
    }
    if current.queued_messages >= 2 {
        return table
    }
    messages: [2]KernelMessage = current.messages
    tail: usize = current.tail
    messages[tail] = message
    tail = advance_index(tail)
    slots[endpoint_index] = EndpointSlot{ endpoint_id: current.endpoint_id, owner_pid: current.owner_pid, head: current.head, tail: tail, queued_messages: current.queued_messages + 1, active: current.active, messages: messages }
    return EndpointTable{ count: table.count, slots: slots }
}

func peek_head_message(table: EndpointTable, endpoint_index: usize) KernelMessage {
    if endpoint_index >= table.count {
        return empty_message()
    }
    current: EndpointSlot = table.slots[endpoint_index]
    if current.queued_messages == 0 {
        return empty_message()
    }
    return current.messages[current.head]
}

func consume_head_message(table: EndpointTable, endpoint_index: usize) EndpointTable {
    if endpoint_index >= table.count {
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