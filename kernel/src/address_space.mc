enum AddressSpaceState {
    Empty,
    Constructed,
    Active,
}

enum MappingKind {
    None,
    ImageText,
    UserStack,
}

struct Mapping {
    base: usize
    size: usize
    kind: MappingKind
    writable: u32
    executable: u32
}

struct AddressSpace {
    asid: u32
    owner_pid: u32
    state: AddressSpaceState
    root_page_table: usize
    entry_pc: usize
    stack_top: usize
    mapping_count: usize
    mappings: [2]Mapping
}

struct UserEntryFrame {
    entry_pc: usize
    stack_top: usize
    address_space_id: u32
    task_id: u32
}

func empty_mapping() Mapping {
    return Mapping{ base: 0, size: 0, kind: MappingKind.None, writable: 0, executable: 0 }
}

func zero_mappings() [2]Mapping {
    mappings: [2]Mapping
    mappings[0] = empty_mapping()
    mappings[1] = empty_mapping()
    return mappings
}

func empty_space() AddressSpace {
    return AddressSpace{ asid: 0, owner_pid: 0, state: AddressSpaceState.Empty, root_page_table: 0, entry_pc: 0, stack_top: 0, mapping_count: 0, mappings: zero_mappings() }
}

func bootstrap_space(asid: u32, owner_pid: u32, root_page_table: usize, image_base: usize, image_size: usize, entry_pc: usize, stack_base: usize, stack_size: usize, stack_top: usize) AddressSpace {
    mappings: [2]Mapping = zero_mappings()
    mappings[0] = Mapping{ base: image_base, size: image_size, kind: MappingKind.ImageText, writable: 0, executable: 1 }
    mappings[1] = Mapping{ base: stack_base, size: stack_size, kind: MappingKind.UserStack, writable: 1, executable: 0 }
    return AddressSpace{ asid: asid, owner_pid: owner_pid, state: AddressSpaceState.Constructed, root_page_table: root_page_table, entry_pc: entry_pc, stack_top: stack_top, mapping_count: 2, mappings: mappings }
}

func activate(space: AddressSpace) AddressSpace {
    return AddressSpace{ asid: space.asid, owner_pid: space.owner_pid, state: AddressSpaceState.Active, root_page_table: space.root_page_table, entry_pc: space.entry_pc, stack_top: space.stack_top, mapping_count: space.mapping_count, mappings: space.mappings }
}

func empty_frame() UserEntryFrame {
    return UserEntryFrame{ entry_pc: 0, stack_top: 0, address_space_id: 0, task_id: 0 }
}

func bootstrap_user_frame(space: AddressSpace, task_id: u32) UserEntryFrame {
    return UserEntryFrame{ entry_pc: space.entry_pc, stack_top: space.stack_top, address_space_id: space.asid, task_id: task_id }
}

func mapping_kind_at(space: AddressSpace, index: usize) MappingKind {
    if index >= space.mapping_count {
        return MappingKind.None
    }
    return space.mappings[index].kind
}

func state_score(state: AddressSpaceState) i32 {
    switch state {
    case AddressSpaceState.Empty:
        return 1
    case AddressSpaceState.Constructed:
        return 2
    case AddressSpaceState.Active:
        return 4
    default:
        return 0
    }
    return 0
}

func kind_score(kind: MappingKind) i32 {
    switch kind {
    case MappingKind.None:
        return 1
    case MappingKind.ImageText:
        return 2
    case MappingKind.UserStack:
        return 4
    default:
        return 0
    }
    return 0
}