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
    slots: [4]EndpointSlot = table.slots
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
    slots: [4]EndpointSlot = table.slots
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