const HANDLE_TABLE_CAPACITY: usize = 5
const HANDLE_NOT_FOUND: usize = 5
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
    Invalidated,
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
    slots: [5]HandleSlot
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

struct EndpointHandleResolution {
    endpoint_id: u32
    valid: u32
}

struct AttachedTransferResolution {
    attached_endpoint_id: u32
    attached_rights: u32
    attached_source_handle_slot: u32
    attached_count: usize
    valid: u32
}

struct ReceivedHandleInstall {
    handle_table: HandleTable
    received_handle_slot: u32
    received_handle_count: usize
    valid: u32
}

struct SpawnWaitHandleAdmission {
    wait_table: WaitTable
    valid: u32
    invalid_handle: u32
    exhausted: u32
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

func zero_handle_slots() [5]HandleSlot {
    slots: [5]HandleSlot
    slots[0] = empty_handle_slot()
    slots[1] = empty_handle_slot()
    slots[2] = empty_handle_slot()
    slots[3] = empty_handle_slot()
    slots[4] = empty_handle_slot()
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