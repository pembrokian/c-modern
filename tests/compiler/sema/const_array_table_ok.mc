struct ServiceSlot {
    endpoint: u32
    pid: u32
}

const SERVICE_SLOTS: [2]ServiceSlot = [2]ServiceSlot{
    ServiceSlot{ endpoint: 10, pid: 1 },
    ServiceSlot{ endpoint: 11, pid: 2 }
}

func first_pid() u32 {
    return SERVICE_SLOTS[0].pid
}

func main() i32 {
    return 0
}