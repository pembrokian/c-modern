const MAPPING_SLOT_COUNT: usize = 2
const MAPPING_NOT_FOUND: usize = 2

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

struct SpawnBootstrap {
    child_address_space: AddressSpace
    child_frame: UserEntryFrame
    valid: u32
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

func mapping_end(base: usize, size: usize) usize {
    return base + size
}

func ranges_overlap(first_base: usize, first_size: usize, second_base: usize, second_size: usize) bool {
    first_end: usize = mapping_end(first_base, first_size)
    second_end: usize = mapping_end(second_base, second_size)
    if first_end <= second_base {
        return false
    }
    if second_end <= first_base {
        return false
    }
    return true
}

func bootstrap_layout_valid(image_base: usize, image_size: usize, entry_pc: usize, stack_base: usize, stack_size: usize, stack_top: usize) bool {
    if image_size == 0 || stack_size == 0 {
        return false
    }
    if entry_pc < image_base {
        return false
    }
    if entry_pc >= mapping_end(image_base, image_size) {
        return false
    }
    if stack_top < stack_base {
        return false
    }
    if stack_top > mapping_end(stack_base, stack_size) {
        return false
    }
    if ranges_overlap(image_base, image_size, stack_base, stack_size) {
        return false
    }
    return true
}

func bootstrap_space_valid(root_page_table: usize, image_base: usize, image_size: usize, entry_pc: usize, stack_base: usize, stack_size: usize, stack_top: usize) bool {
    if root_page_table == 0 {
        return false
    }
    return bootstrap_layout_valid(image_base, image_size, entry_pc, stack_base, stack_size, stack_top)
}

func bootstrap_space(asid: u32, owner_pid: u32, root_page_table: usize, image_base: usize, image_size: usize, entry_pc: usize, stack_base: usize, stack_size: usize, stack_top: usize) AddressSpace {
    if !bootstrap_space_valid(root_page_table, image_base, image_size, entry_pc, stack_base, stack_size, stack_top) {
        return empty_space()
    }
    mappings: [2]Mapping = zero_mappings()
    mappings[0] = Mapping{ base: image_base, size: image_size, kind: MappingKind.ImageText, writable: 0, executable: 1 }
    mappings[1] = Mapping{ base: stack_base, size: stack_size, kind: MappingKind.UserStack, writable: 1, executable: 0 }
    return AddressSpace{ asid: asid, owner_pid: owner_pid, state: AddressSpaceState.Constructed, root_page_table: root_page_table, entry_pc: entry_pc, stack_top: stack_top, mapping_count: 2, mappings: mappings }
}

func can_activate(space: AddressSpace) bool {
    if state_score(space.state) != 2 {
        return false
    }
    if space.root_page_table == 0 {
        return false
    }
    if space.mapping_count != MAPPING_SLOT_COUNT {
        return false
    }
    return true
}

func activate(space: AddressSpace) AddressSpace {
    if !can_activate(space) {
        return space
    }
    return AddressSpace{ asid: space.asid, owner_pid: space.owner_pid, state: AddressSpaceState.Active, root_page_table: space.root_page_table, entry_pc: space.entry_pc, stack_top: space.stack_top, mapping_count: space.mapping_count, mappings: space.mappings }
}

func empty_frame() UserEntryFrame {
    return UserEntryFrame{ entry_pc: 0, stack_top: 0, address_space_id: 0, task_id: 0 }
}

func bootstrap_user_frame(space: AddressSpace, task_id: u32) UserEntryFrame {
    return UserEntryFrame{ entry_pc: space.entry_pc, stack_top: space.stack_top, address_space_id: space.asid, task_id: task_id }
}

func build_child_bootstrap_context(owner_pid: u32, child_tid: u32, child_asid: u32, root_page_table: usize, image_base: usize, image_size: usize, entry_pc: usize, stack_base: usize, stack_size: usize, stack_top: usize) SpawnBootstrap {
    child_space: AddressSpace = bootstrap_space(child_asid, owner_pid, root_page_table, image_base, image_size, entry_pc, stack_base, stack_size, stack_top)
    if state_score(child_space.state) != 2 {
        return SpawnBootstrap{ child_address_space: empty_space(), child_frame: empty_frame(), valid: 0 }
    }
    child_frame: UserEntryFrame = bootstrap_user_frame(child_space, child_tid)
    return SpawnBootstrap{ child_address_space: child_space, child_frame: child_frame, valid: 1 }
}

func validate_syscall_address_space_boundary() bool {
    child: SpawnBootstrap = build_child_bootstrap_context(3, 4, 5, 32768, 65536, 8192, 66048, 131072, 8192, 139264)
    if child.valid == 0 {
        return false
    }
    if child.child_address_space.owner_pid != 3 {
        return false
    }
    if child.child_frame.task_id != 4 {
        return false
    }
    if child.child_frame.address_space_id != 5 {
        return false
    }
    return true
}

func mapping_kind_at(space: AddressSpace, index: usize) MappingKind {
    if index >= space.mapping_count {
        return MappingKind.None
    }
    return space.mappings[index].kind
}

func find_mapping_index(space: AddressSpace, kind: MappingKind) usize {
    if space.mapping_count == 0 {
        return MAPPING_NOT_FOUND
    }
    if kind_score(space.mappings[0].kind) == kind_score(kind) {
        return 0
    }
    if space.mapping_count < MAPPING_SLOT_COUNT {
        return MAPPING_NOT_FOUND
    }
    if kind_score(space.mappings[1].kind) == kind_score(kind) {
        return 1
    }
    return MAPPING_NOT_FOUND
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