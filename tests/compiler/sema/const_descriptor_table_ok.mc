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

const DESCRIPTORS: [2]ServiceDescriptor = {
    { label: "serial", slot: { endpoint: 10, pid: 1 }, restart: RestartMode.None, authority: AuthorityClass.PublicEndpoint },
    { label: "journal", slot: { endpoint: 21, pid: 12 }, restart: RestartMode.Reload, authority: AuthorityClass.DurableOwner }
}

func second_endpoint() u32 {
    return DESCRIPTORS[1].slot.endpoint
}

func main() i32 {
    return 0
}