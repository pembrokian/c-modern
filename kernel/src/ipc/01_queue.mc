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
    slots[endpoint_index] = EndpointSlot{ endpoint_id: current.endpoint_id, owner_pid: current.owner_pid, head: current.head, tail: tail, queued_messages: current.queued_messages + 1, active: current.active, blocked_sender: current.blocked_sender, blocked_receiver: current.blocked_receiver, messages: messages }
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
    slots[endpoint_index] = EndpointSlot{ endpoint_id: current.endpoint_id, owner_pid: current.owner_pid, head: head, tail: tail, queued_messages: count, active: current.active, blocked_sender: current.blocked_sender, blocked_receiver: current.blocked_receiver, messages: messages }
    return EndpointTable{ count: table.count, slots: slots }
}

func register_blocked_sender(table: EndpointTable, endpoint_index: usize, task_slot: u32, task_id: u32) EndpointTable {
    if !endpoint_index_valid(table, endpoint_index) {
        return table
    }
    slots: [4]EndpointSlot = table.slots
    current: EndpointSlot = slots[endpoint_index]
    slots[endpoint_index] = EndpointSlot{ endpoint_id: current.endpoint_id, owner_pid: current.owner_pid, head: current.head, tail: current.tail, queued_messages: current.queued_messages, active: current.active, blocked_sender: EndpointWaiter{ task_slot: task_slot, task_id: task_id, waiting: 1 }, blocked_receiver: current.blocked_receiver, messages: current.messages }
    return EndpointTable{ count: table.count, slots: slots }
}

func register_blocked_receiver(table: EndpointTable, endpoint_index: usize, task_slot: u32, task_id: u32) EndpointTable {
    if !endpoint_index_valid(table, endpoint_index) {
        return table
    }
    slots: [4]EndpointSlot = table.slots
    current: EndpointSlot = slots[endpoint_index]
    slots[endpoint_index] = EndpointSlot{ endpoint_id: current.endpoint_id, owner_pid: current.owner_pid, head: current.head, tail: current.tail, queued_messages: current.queued_messages, active: current.active, blocked_sender: current.blocked_sender, blocked_receiver: EndpointWaiter{ task_slot: task_slot, task_id: task_id, waiting: 1 }, messages: current.messages }
    return EndpointTable{ count: table.count, slots: slots }
}

func take_blocked_sender(table: EndpointTable, endpoint_index: usize, wake_reason: EndpointWakeReason) EndpointWakeTransition {
    if !endpoint_index_valid(table, endpoint_index) {
        return EndpointWakeTransition{ endpoints: table, wake: no_wake() }
    }
    slots: [4]EndpointSlot = table.slots
    current: EndpointSlot = slots[endpoint_index]
    if current.blocked_sender.waiting == 0 {
        return EndpointWakeTransition{ endpoints: table, wake: no_wake() }
    }
    wake: EndpointWake = EndpointWake{ task_slot: current.blocked_sender.task_slot, task_id: current.blocked_sender.task_id, wake_reason: wake_reason, woke: 1 }
    slots[endpoint_index] = EndpointSlot{ endpoint_id: current.endpoint_id, owner_pid: current.owner_pid, head: current.head, tail: current.tail, queued_messages: current.queued_messages, active: current.active, blocked_sender: empty_waiter(), blocked_receiver: current.blocked_receiver, messages: current.messages }
    return EndpointWakeTransition{ endpoints: EndpointTable{ count: table.count, slots: slots }, wake: wake }
}

func take_blocked_receiver(table: EndpointTable, endpoint_index: usize, wake_reason: EndpointWakeReason) EndpointWakeTransition {
    if !endpoint_index_valid(table, endpoint_index) {
        return EndpointWakeTransition{ endpoints: table, wake: no_wake() }
    }
    slots: [4]EndpointSlot = table.slots
    current: EndpointSlot = slots[endpoint_index]
    if current.blocked_receiver.waiting == 0 {
        return EndpointWakeTransition{ endpoints: table, wake: no_wake() }
    }
    wake: EndpointWake = EndpointWake{ task_slot: current.blocked_receiver.task_slot, task_id: current.blocked_receiver.task_id, wake_reason: wake_reason, woke: 1 }
    slots[endpoint_index] = EndpointSlot{ endpoint_id: current.endpoint_id, owner_pid: current.owner_pid, head: current.head, tail: current.tail, queued_messages: current.queued_messages, active: current.active, blocked_sender: current.blocked_sender, blocked_receiver: empty_waiter(), messages: current.messages }
    return EndpointWakeTransition{ endpoints: EndpointTable{ count: table.count, slots: slots }, wake: wake }
}