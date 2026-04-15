struct ServiceSlot {
    endpoint: u32
    pid: u32
}

const SERVICE_SLOTS: [2]ServiceSlot = {
    { endpoint: 10, pid: 1 },
    { endpoint: 11, pid: 2 }
}

func make_slot() ServiceSlot {
    return { endpoint: 12, pid: 3 }
}

func slot_pid(slot: ServiceSlot) u32 {
    return slot.pid
}

func third_pid() u32 {
    return slot_pid({ endpoint: 12, pid: 3 })
}

func main() i32 {
    return 0
}