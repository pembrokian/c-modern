const HANDLE_TABLE_CAPACITY: usize = 4
const HANDLE_NOT_FOUND: usize = 4
const WAIT_TABLE_CAPACITY: usize = 2
const WAIT_NOT_FOUND: usize = 2
const RIGHTS_ENDPOINT_SEND: u32 = 1
const RIGHTS_ENDPOINT_RECEIVE: u32 = 2
const RIGHTS_ENDPOINT_TRANSFER: u32 = 4
const RIGHTS_ENDPOINT_ALL: u32 = 7
const RIGHTS_ALL: u32 = 255
const RIGHTS_INIT_PROGRAM: u32 = 7

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

enum WaitHandleState {
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
    slots: [4]HandleSlot
}

struct WaitHandle {
    slot_id: u32
    owner_pid: u32
    child_pid: u32
    exit_code: i32
    signaled: u32
    state: WaitHandleState
}

struct WaitTable {
    owner_pid: u32
    count: usize
    slots: [2]WaitHandle
}

func empty_slot() CapabilitySlot {
    return CapabilitySlot{ slot_id: 0, owner_pid: 0, kind: CapabilityKind.Empty, rights: 0, object_id: 0 }
}

func empty_handle_slot() HandleSlot {
    return HandleSlot{ slot_id: 0, owner_pid: 0, endpoint_id: 0, rights: 0, state: HandleState.Empty }
}

func empty_wait_handle() WaitHandle {
    return WaitHandle{ slot_id: 0, owner_pid: 0, child_pid: 0, exit_code: 0, signaled: 0, state: WaitHandleState.Empty }
}

func zero_handle_slots() [4]HandleSlot {
    slots: [4]HandleSlot
    slots[0] = empty_handle_slot()
    slots[1] = empty_handle_slot()
    slots[2] = empty_handle_slot()
    slots[3] = empty_handle_slot()
    return slots
}

func empty_handle_table() HandleTable {
    return HandleTable{ owner_pid: 0, count: 0, slots: zero_handle_slots() }
}

func zero_wait_handles() [2]WaitHandle {
    slots: [2]WaitHandle
    slots[0] = empty_wait_handle()
    slots[1] = empty_wait_handle()
    return slots
}

func empty_wait_table() WaitTable {
    return WaitTable{ owner_pid: 0, count: 0, slots: zero_wait_handles() }
}

func handle_table_for_owner(owner_pid: u32) HandleTable {
    return HandleTable{ owner_pid: owner_pid, count: 0, slots: zero_handle_slots() }
}

func wait_table_for_owner(owner_pid: u32) WaitTable {
    return WaitTable{ owner_pid: owner_pid, count: 0, slots: zero_wait_handles() }
}

func zero_handle_tables() [3]HandleTable {
    tables: [3]HandleTable
    tables[0] = empty_handle_table()
    tables[1] = empty_handle_table()
    tables[2] = empty_handle_table()
    return tables
}

func zero_wait_tables() [3]WaitTable {
    tables: [3]WaitTable
    tables[0] = empty_wait_table()
    tables[1] = empty_wait_table()
    tables[2] = empty_wait_table()
    return tables
}

func bootstrap_root_slot(owner_pid: u32) CapabilitySlot {
    return CapabilitySlot{ slot_id: 1, owner_pid: owner_pid, kind: CapabilityKind.Root, rights: RIGHTS_ALL, object_id: 1 }
}

func bootstrap_init_program_slot(owner_pid: u32) CapabilitySlot {
    return CapabilitySlot{ slot_id: 2, owner_pid: owner_pid, kind: CapabilityKind.InitProgram, rights: RIGHTS_INIT_PROGRAM, object_id: 1 }
}

func is_program_capability(slot: CapabilitySlot) bool {
    return kind_score(slot.kind) == 4
}

func find_handle_index(table: HandleTable, slot_id: u32) usize {
    if handle_state_score(table.slots[0].state) == 2 && table.slots[0].slot_id == slot_id {
        return 0
    }
    if handle_state_score(table.slots[1].state) == 2 && table.slots[1].slot_id == slot_id {
        return 1
    }
    if handle_state_score(table.slots[2].state) == 2 && table.slots[2].slot_id == slot_id {
        return 2
    }
    if handle_state_score(table.slots[3].state) == 2 && table.slots[3].slot_id == slot_id {
        return 3
    }
    return HANDLE_NOT_FOUND
}

func find_wait_index(table: WaitTable, slot_id: u32) usize {
    if wait_handle_state_score(table.slots[0].state) == 2 && table.slots[0].slot_id == slot_id {
        return 0
    }
    if wait_handle_state_score(table.slots[1].state) == 2 && table.slots[1].slot_id == slot_id {
        return 1
    }
    return WAIT_NOT_FOUND
}

func handle_slot_installed(table: HandleTable, slot_id: u32) bool {
    return find_handle_index(table, slot_id) < HANDLE_TABLE_CAPACITY
}

func wait_handle_installed(table: WaitTable, slot_id: u32) bool {
    return find_wait_index(table, slot_id) < WAIT_TABLE_CAPACITY
}

func handle_install_succeeded(before: HandleTable, after: HandleTable, slot_id: u32) bool {
    if after.count != before.count + 1 {
        return false
    }
    return handle_slot_installed(after, slot_id)
}

func wait_install_succeeded(before: WaitTable, after: WaitTable, slot_id: u32) bool {
    if after.count != before.count + 1 {
        return false
    }
    return wait_handle_installed(after, slot_id)
}

func handle_remove_succeeded(before: HandleTable, after: HandleTable, slot_id: u32) bool {
    if before.count == 0 {
        return false
    }
    if after.count + 1 != before.count {
        return false
    }
    return !handle_slot_installed(after, slot_id)
}

func endpoint_rights_valid(rights: u32) bool {
    if rights == 0 {
        return false
    }
    if rights > RIGHTS_ENDPOINT_ALL {
        return false
    }
    return true
}

func attenuate_endpoint_rights(rights: u32) u32 {
    if !endpoint_rights_valid(rights) {
        return 0
    }
    return rights
}

func find_transfer_rights_for_handle(table: HandleTable, slot_id: u32) u32 {
    return attenuate_endpoint_rights(find_rights_for_handle(table, slot_id))
}

func install_endpoint_handle(table: HandleTable, slot_id: u32, endpoint_id: u32, rights: u32) HandleTable {
    if table.count >= HANDLE_TABLE_CAPACITY {
        return table
    }
    if !endpoint_rights_valid(rights) {
        return table
    }
    if handle_slot_installed(table, slot_id) {
        return table
    }
    slots: [4]HandleSlot = table.slots
    if handle_state_score(slots[0].state) == 1 {
        slots[0] = HandleSlot{ slot_id: slot_id, owner_pid: table.owner_pid, endpoint_id: endpoint_id, rights: rights, state: HandleState.Installed }
        return HandleTable{ owner_pid: table.owner_pid, count: table.count + 1, slots: slots }
    }
    if handle_state_score(slots[1].state) == 1 {
        slots[1] = HandleSlot{ slot_id: slot_id, owner_pid: table.owner_pid, endpoint_id: endpoint_id, rights: rights, state: HandleState.Installed }
        return HandleTable{ owner_pid: table.owner_pid, count: table.count + 1, slots: slots }
    }
    if handle_state_score(slots[2].state) == 1 {
        slots[2] = HandleSlot{ slot_id: slot_id, owner_pid: table.owner_pid, endpoint_id: endpoint_id, rights: rights, state: HandleState.Installed }
        return HandleTable{ owner_pid: table.owner_pid, count: table.count + 1, slots: slots }
    }
    if handle_state_score(slots[3].state) == 1 {
        slots[3] = HandleSlot{ slot_id: slot_id, owner_pid: table.owner_pid, endpoint_id: endpoint_id, rights: rights, state: HandleState.Installed }
        return HandleTable{ owner_pid: table.owner_pid, count: table.count + 1, slots: slots }
    }
    return table
}

func remove_handle(table: HandleTable, slot_id: u32) HandleTable {
    index: usize = find_handle_index(table, slot_id)
    if index >= HANDLE_TABLE_CAPACITY || table.count == 0 {
        return table
    }
    slots: [4]HandleSlot = table.slots
    slots[index] = empty_handle_slot()
    return HandleTable{ owner_pid: table.owner_pid, count: table.count - 1, slots: slots }
}

func install_wait_handle(table: WaitTable, slot_id: u32, child_pid: u32) WaitTable {
    if table.count >= WAIT_TABLE_CAPACITY {
        return table
    }
    if wait_handle_installed(table, slot_id) {
        return table
    }
    slots: [2]WaitHandle = table.slots
    if wait_handle_state_score(slots[0].state) == 1 {
        slots[0] = WaitHandle{ slot_id: slot_id, owner_pid: table.owner_pid, child_pid: child_pid, exit_code: 0, signaled: 0, state: WaitHandleState.Installed }
        return WaitTable{ owner_pid: table.owner_pid, count: table.count + 1, slots: slots }
    }
    if wait_handle_state_score(slots[1].state) == 1 {
        slots[1] = WaitHandle{ slot_id: slot_id, owner_pid: table.owner_pid, child_pid: child_pid, exit_code: 0, signaled: 0, state: WaitHandleState.Installed }
        return WaitTable{ owner_pid: table.owner_pid, count: table.count + 1, slots: slots }
    }
    return table
}

func mark_wait_handle_exited(table: WaitTable, child_pid: u32, exit_code: i32) WaitTable {
    slots: [2]WaitHandle = table.slots
    if wait_handle_state_score(slots[0].state) == 2 && slots[0].child_pid == child_pid {
        slots[0] = WaitHandle{ slot_id: slots[0].slot_id, owner_pid: slots[0].owner_pid, child_pid: child_pid, exit_code: exit_code, signaled: 1, state: slots[0].state }
        return WaitTable{ owner_pid: table.owner_pid, count: table.count, slots: slots }
    }
    if wait_handle_state_score(slots[1].state) == 2 && slots[1].child_pid == child_pid {
        slots[1] = WaitHandle{ slot_id: slots[1].slot_id, owner_pid: slots[1].owner_pid, child_pid: child_pid, exit_code: exit_code, signaled: 1, state: slots[1].state }
        return WaitTable{ owner_pid: table.owner_pid, count: table.count, slots: slots }
    }
    return table
}

func find_child_for_wait_handle(table: WaitTable, slot_id: u32) u32 {
    index: usize = find_wait_index(table, slot_id)
    if index >= 2 {
        return 0
    }
    return table.slots[index].child_pid
}

func wait_handle_signaled(table: WaitTable, slot_id: u32) u32 {
    index: usize = find_wait_index(table, slot_id)
    if index >= 2 {
        return 0
    }
    return table.slots[index].signaled
}

func find_exit_code_for_wait_handle(table: WaitTable, slot_id: u32) i32 {
    index: usize = find_wait_index(table, slot_id)
    if index >= 2 {
        return 0
    }
    return table.slots[index].exit_code
}

func consume_wait_handle(table: WaitTable, slot_id: u32) WaitTable {
    index: usize = find_wait_index(table, slot_id)
    if index >= WAIT_TABLE_CAPACITY || table.count == 0 {
        return table
    }
    slots: [2]WaitHandle = table.slots
    slots[index] = empty_wait_handle()
    return WaitTable{ owner_pid: table.owner_pid, count: table.count - 1, slots: slots }
}

func find_endpoint_for_handle(table: HandleTable, slot_id: u32) u32 {
    index: usize = find_handle_index(table, slot_id)
    if index >= HANDLE_TABLE_CAPACITY {
        return 0
    }
    return table.slots[index].endpoint_id
}

func find_rights_for_handle(table: HandleTable, slot_id: u32) u32 {
    index: usize = find_handle_index(table, slot_id)
    if index >= HANDLE_TABLE_CAPACITY {
        return 0
    }
    return table.slots[index].rights
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

func wait_handle_state_score(state: WaitHandleState) i32 {
    switch state {
    case WaitHandleState.Empty:
        return 1
    case WaitHandleState.Installed:
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