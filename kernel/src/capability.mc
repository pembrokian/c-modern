enum CapabilityKind {
    Empty,
    Root,
    InitProgram,
    Endpoint,
}

enum HandleState {
    Empty,
    Installed,
}

struct CapabilitySlot {
    slot_id: u32
    owner_pid: u32
    kind: CapabilityKind
    rights: u32
    object_id: u32
}

struct HandleSlot {
    slot_id: u32
    owner_pid: u32
    endpoint_id: u32
    rights: u32
    state: HandleState
}

struct HandleTable {
    owner_pid: u32
    count: usize
    slots: [2]HandleSlot
}

func empty_slot() CapabilitySlot {
    return CapabilitySlot{ slot_id: 0, owner_pid: 0, kind: CapabilityKind.Empty, rights: 0, object_id: 0 }
}

func empty_handle_slot() HandleSlot {
    return HandleSlot{ slot_id: 0, owner_pid: 0, endpoint_id: 0, rights: 0, state: HandleState.Empty }
}

func zero_handle_slots() [2]HandleSlot {
    slots: [2]HandleSlot
    slots[0] = empty_handle_slot()
    slots[1] = empty_handle_slot()
    return slots
}

func empty_handle_table() HandleTable {
    return HandleTable{ owner_pid: 0, count: 0, slots: zero_handle_slots() }
}

func handle_table_for_owner(owner_pid: u32) HandleTable {
    return HandleTable{ owner_pid: owner_pid, count: 0, slots: zero_handle_slots() }
}

func zero_handle_tables() [2]HandleTable {
    tables: [2]HandleTable
    tables[0] = empty_handle_table()
    tables[1] = empty_handle_table()
    return tables
}

func bootstrap_root_slot(owner_pid: u32) CapabilitySlot {
    return CapabilitySlot{ slot_id: 1, owner_pid: owner_pid, kind: CapabilityKind.Root, rights: 255, object_id: 1 }
}

func bootstrap_init_program_slot(owner_pid: u32) CapabilitySlot {
    return CapabilitySlot{ slot_id: 2, owner_pid: owner_pid, kind: CapabilityKind.InitProgram, rights: 7, object_id: 1 }
}

func install_endpoint_handle(table: HandleTable, slot_id: u32, endpoint_id: u32, rights: u32) HandleTable {
    if table.count >= 2 {
        return table
    }
    slots: [2]HandleSlot = table.slots
    slots[table.count] = HandleSlot{ slot_id: slot_id, owner_pid: table.owner_pid, endpoint_id: endpoint_id, rights: rights, state: HandleState.Installed }
    return HandleTable{ owner_pid: table.owner_pid, count: table.count + 1, slots: slots }
}

func find_endpoint_for_handle(table: HandleTable, slot_id: u32) u32 {
    if table.count == 0 {
        return 0
    }
    if table.slots[0].slot_id == slot_id && handle_state_score(table.slots[0].state) == 2 {
        return table.slots[0].endpoint_id
    }
    if table.count < 2 {
        return 0
    }
    if table.slots[1].slot_id == slot_id && handle_state_score(table.slots[1].state) == 2 {
        return table.slots[1].endpoint_id
    }
    return 0
 }

func handle_state_score(state: HandleState) i32 {
    switch state {
    case HandleState.Empty:
        return 1
    case HandleState.Installed:
        return 2
    default:
        return 0
    }
    return 0
}

func kind_score(kind: CapabilityKind) i32 {
    switch kind {
    case CapabilityKind.Empty:
        return 1
    case CapabilityKind.Root:
        return 2
    case CapabilityKind.InitProgram:
        return 4
    case CapabilityKind.Endpoint:
        return 8
    default:
        return 0
    }
    return 0
}