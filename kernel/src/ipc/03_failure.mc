func scrub_live_messages(messages: [2]KernelMessage) AbortMessagesResult {
    aborted_messages: usize = 0
    if message_state_score(messages[0].state) != 1 {
        aborted_messages = aborted_messages + 1
    }
    if message_state_score(messages[1].state) != 1 {
        aborted_messages = aborted_messages + 1
    }
    return AbortMessagesResult{ messages: zero_messages(), aborted_messages: aborted_messages }
}

func close_endpoint_slot(slot: EndpointSlot) EndpointCloseSlotTransition {
    if slot.active == 0 {
        return EndpointCloseSlotTransition{ slot: slot, wakes: empty_wake_list(), aborted_messages: 0, closed: 0 }
    }
    wakes: EndpointWakeList = empty_wake_list()
    if slot.blocked_sender.waiting == 1 {
        wakes = append_wake(wakes, EndpointWake{ task_slot: slot.blocked_sender.task_slot, task_id: slot.blocked_sender.task_id, wake_reason: EndpointWakeReason.EndpointClosed, woke: 1 })
    }
    if slot.blocked_receiver.waiting == 1 {
        wakes = append_wake(wakes, EndpointWake{ task_slot: slot.blocked_receiver.task_slot, task_id: slot.blocked_receiver.task_id, wake_reason: EndpointWakeReason.EndpointClosed, woke: 1 })
    }
    scrubbed: AbortMessagesResult = scrub_live_messages(slot.messages)
    return EndpointCloseSlotTransition{ slot: EndpointSlot{ endpoint_id: slot.endpoint_id, owner_pid: slot.owner_pid, head: 0, tail: 0, queued_messages: 0, active: 0, blocked_sender: empty_waiter(), blocked_receiver: empty_waiter(), messages: scrubbed.messages }, wakes: wakes, aborted_messages: scrubbed.aborted_messages, closed: 1 }
}

func close_runtime_endpoint(table: EndpointTable, endpoint_id: u32) EndpointCloseTransition {
    lookup: EndpointLookup = lookup_runtime_endpoint(table, endpoint_id)
    if lookup.found == 0 {
        return EndpointCloseTransition{ endpoints: table, endpoint_id: endpoint_id, wakes: empty_wake_list(), aborted_messages: 0, endpoint_found: 0, closed: 0 }
    }
    slots: [4]EndpointSlot = table.slots
    closed_slot: EndpointCloseSlotTransition = close_endpoint_slot(slots[lookup.endpoint_index])
    slots[lookup.endpoint_index] = closed_slot.slot
    return EndpointCloseTransition{ endpoints: EndpointTable{ count: table.count, slots: slots }, endpoint_id: endpoint_id, wakes: closed_slot.wakes, aborted_messages: closed_slot.aborted_messages, endpoint_found: 1, closed: closed_slot.closed }
}

func close_runtime_endpoints_for_owner(table: EndpointTable, owner_pid: u32) EndpointOwnerCloseTransition {
    endpoints: EndpointTable = table
    wakes: EndpointWakeList = empty_wake_list()
    aborted_messages: usize = 0
    closed_count: usize = 0

    if endpoints.count > 0 && endpoints.slots[0].owner_pid == owner_pid && endpoints.slots[0].active == 1 {
        closed_zero: EndpointCloseTransition = close_runtime_endpoint(endpoints, endpoints.slots[0].endpoint_id)
        endpoints = closed_zero.endpoints
        wakes = append_wake(wakes, closed_zero.wakes.wakes[0])
        wakes = append_wake(wakes, closed_zero.wakes.wakes[1])
        aborted_messages = aborted_messages + closed_zero.aborted_messages
        if closed_zero.closed == 1 {
            closed_count = closed_count + 1
        }
    }
    if endpoints.count > 1 && endpoints.slots[1].owner_pid == owner_pid && endpoints.slots[1].active == 1 {
        closed_one: EndpointCloseTransition = close_runtime_endpoint(endpoints, endpoints.slots[1].endpoint_id)
        endpoints = closed_one.endpoints
        wakes = append_wake(wakes, closed_one.wakes.wakes[0])
        wakes = append_wake(wakes, closed_one.wakes.wakes[1])
        aborted_messages = aborted_messages + closed_one.aborted_messages
        if closed_one.closed == 1 {
            closed_count = closed_count + 1
        }
    }
    if endpoints.count > 2 && endpoints.slots[2].owner_pid == owner_pid && endpoints.slots[2].active == 1 {
        closed_two: EndpointCloseTransition = close_runtime_endpoint(endpoints, endpoints.slots[2].endpoint_id)
        endpoints = closed_two.endpoints
        wakes = append_wake(wakes, closed_two.wakes.wakes[0])
        wakes = append_wake(wakes, closed_two.wakes.wakes[1])
        aborted_messages = aborted_messages + closed_two.aborted_messages
        if closed_two.closed == 1 {
            closed_count = closed_count + 1
        }
    }
    if endpoints.count > 3 && endpoints.slots[3].owner_pid == owner_pid && endpoints.slots[3].active == 1 {
        closed_three: EndpointCloseTransition = close_runtime_endpoint(endpoints, endpoints.slots[3].endpoint_id)
        endpoints = closed_three.endpoints
        wakes = append_wake(wakes, closed_three.wakes.wakes[0])
        wakes = append_wake(wakes, closed_three.wakes.wakes[1])
        aborted_messages = aborted_messages + closed_three.aborted_messages
        if closed_three.closed == 1 {
            closed_count = closed_count + 1
        }
    }

    return EndpointOwnerCloseTransition{ endpoints: endpoints, wakes: wakes, aborted_messages: aborted_messages, closed_count: closed_count }
}