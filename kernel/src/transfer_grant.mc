import syscall

const GRANT_CAPACITY: usize = 4

struct GrantTable {
    owners: [GRANT_CAPACITY]u32
    endpoints: [GRANT_CAPACITY]u32
    ids: [GRANT_CAPACITY]u32
    next: u32
}

struct GrantIssue {
    table: GrantTable
    slot: u32
    status: syscall.SyscallStatus
}

struct GrantConsume {
    table: GrantTable
    endpoint0: u32
    endpoint1: u32
    status: syscall.SyscallStatus
}

func grant_init() GrantTable {
    owners: [GRANT_CAPACITY]u32
    owners[0] = 0
    owners[1] = 0
    owners[2] = 0
    owners[3] = 0
    endpoints: [GRANT_CAPACITY]u32
    endpoints[0] = 0
    endpoints[1] = 0
    endpoints[2] = 0
    endpoints[3] = 0
    ids: [GRANT_CAPACITY]u32
    ids[0] = 0
    ids[1] = 0
    ids[2] = 0
    ids[3] = 0
    return GrantTable{ owners: owners, endpoints: endpoints, ids: ids, next: 1 }
}

func grantwith(t: GrantTable, owners: [GRANT_CAPACITY]u32, endpoints: [GRANT_CAPACITY]u32, ids: [GRANT_CAPACITY]u32, next: u32) GrantTable {
    return GrantTable{ owners: owners, endpoints: endpoints, ids: ids, next: next }
}

func grant_issue(t: GrantTable, pid: u32, endpoint: u32) GrantIssue {
    for i in 0..GRANT_CAPACITY {
        if t.endpoints[i] == 0 {
            next_owners: [GRANT_CAPACITY]u32 = t.owners
            next_endpoints: [GRANT_CAPACITY]u32 = t.endpoints
            next_ids: [GRANT_CAPACITY]u32 = t.ids
            next_owners[i] = pid
            next_endpoints[i] = endpoint
            next_ids[i] = t.next
            return GrantIssue{ table: grantwith(t, next_owners, next_endpoints, next_ids, t.next + 1), slot: t.next, status: syscall.SyscallStatus.Ok }
        }
    }
    return GrantIssue{ table: t, slot: 0, status: syscall.SyscallStatus.Exhausted }
}

func grant_consume(t: GrantTable, pid: u32, slot: u32, count: usize) GrantConsume {
    if count == 1 {
        return grant_consume_one(t, pid, slot)
    }
    if count == 2 {
        return grant_consume_two(t, pid, slot)
    }
    return GrantConsume{ table: t, endpoint0: 0, endpoint1: 0, status: syscall.SyscallStatus.InvalidCapability }
}

func grant_consume_one(t: GrantTable, pid: u32, slot: u32) GrantConsume {
    for i in 0..GRANT_CAPACITY {
        if t.ids[i] == slot {
            return grant_consume_index(t, pid, i)
        }
    }
    return GrantConsume{ table: t, endpoint0: 0, endpoint1: 0, status: syscall.SyscallStatus.InvalidCapability }
}

func grant_consume_two(t: GrantTable, pid: u32, slot: u32) GrantConsume {
    first: usize = GRANT_CAPACITY
    second: usize = GRANT_CAPACITY
    for i in 0..GRANT_CAPACITY {
        if t.ids[i] == slot {
            first = i
        }
        if t.ids[i] == slot + 1 {
            second = i
        }
    }
    if first >= GRANT_CAPACITY {
        return GrantConsume{ table: t, endpoint0: 0, endpoint1: 0, status: syscall.SyscallStatus.InvalidCapability }
    }
    if second >= GRANT_CAPACITY {
        return GrantConsume{ table: t, endpoint0: 0, endpoint1: 0, status: syscall.SyscallStatus.InvalidCapability }
    }
    return grant_consume_pair_index(t, pid, first, second)
}

func grant_consume_index(t: GrantTable, pid: u32, index: usize) GrantConsume {
    if t.endpoints[index] == 0 {
        return GrantConsume{ table: t, endpoint0: 0, endpoint1: 0, status: syscall.SyscallStatus.InvalidCapability }
    }
    if t.owners[index] != pid {
        return GrantConsume{ table: t, endpoint0: 0, endpoint1: 0, status: syscall.SyscallStatus.InvalidCapability }
    }
    next_owners: [GRANT_CAPACITY]u32 = t.owners
    next_endpoints: [GRANT_CAPACITY]u32 = t.endpoints
    next_ids: [GRANT_CAPACITY]u32 = t.ids
    endpoint: u32 = next_endpoints[index]
    next_owners[index] = 0
    next_endpoints[index] = 0
    next_ids[index] = 0
    return GrantConsume{ table: grantwith(t, next_owners, next_endpoints, next_ids, t.next), endpoint0: endpoint, endpoint1: 0, status: syscall.SyscallStatus.Ok }
}

func grant_consume_pair_index(t: GrantTable, pid: u32, first: usize, second: usize) GrantConsume {
    if first == second {
        return GrantConsume{ table: t, endpoint0: 0, endpoint1: 0, status: syscall.SyscallStatus.InvalidCapability }
    }
    if t.endpoints[first] == 0 {
        return GrantConsume{ table: t, endpoint0: 0, endpoint1: 0, status: syscall.SyscallStatus.InvalidCapability }
    }
    if t.endpoints[second] == 0 {
        return GrantConsume{ table: t, endpoint0: 0, endpoint1: 0, status: syscall.SyscallStatus.InvalidCapability }
    }
    if t.owners[first] != pid {
        return GrantConsume{ table: t, endpoint0: 0, endpoint1: 0, status: syscall.SyscallStatus.InvalidCapability }
    }
    if t.owners[second] != pid {
        return GrantConsume{ table: t, endpoint0: 0, endpoint1: 0, status: syscall.SyscallStatus.InvalidCapability }
    }
    next_owners: [GRANT_CAPACITY]u32 = t.owners
    next_endpoints: [GRANT_CAPACITY]u32 = t.endpoints
    next_ids: [GRANT_CAPACITY]u32 = t.ids
    endpoint0: u32 = next_endpoints[first]
    endpoint1: u32 = next_endpoints[second]
    next_owners[first] = 0
    next_endpoints[first] = 0
    next_ids[first] = 0
    next_owners[second] = 0
    next_endpoints[second] = 0
    next_ids[second] = 0
    return GrantConsume{ table: grantwith(t, next_owners, next_endpoints, next_ids, t.next), endpoint0: endpoint0, endpoint1: endpoint1, status: syscall.SyscallStatus.Ok }
}