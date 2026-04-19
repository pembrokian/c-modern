enum RestartMode {
    None,
    Reload,
    Reset,
}

enum AuthorityClass {
    None,
    PublicEndpoint,
    DurableOwner,
}

struct ServiceSlot {
    endpoint: u32
    pid: u32
}

struct ServiceDescriptor {
    label: str
    slot: ServiceSlot
    restart: RestartMode
    authority: AuthorityClass
}

func second_score() i32 {
    descriptors: [2]ServiceDescriptor = {
        { label: "serial", slot: { endpoint: 10, pid: 1 }, restart: RestartMode.None, authority: AuthorityClass.PublicEndpoint },
        { label: "journal", slot: { endpoint: 21, pid: 12 }, restart: RestartMode.Reload, authority: AuthorityClass.DurableOwner }
    }
    desc: ServiceDescriptor = descriptors[1]
    if desc.restart != RestartMode.Reload {
        return 1
    }
    if desc.authority != AuthorityClass.DurableOwner {
        return 2
    }
    return i32(desc.slot.endpoint) + i32(desc.slot.pid) + i32(desc.label.len)
}

func main() i32 {
    return second_score()
}