struct EndpointSlot {
    endpoint_id: u32
    owner_pid: u32
    queued_messages: u32
    active: u32
}

struct EndpointTable {
    count: usize
    slots: [2]EndpointSlot
}

func empty_slot() EndpointSlot {
    return EndpointSlot{ endpoint_id: 0, owner_pid: 0, queued_messages: 0, active: 0 }
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