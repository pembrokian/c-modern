enum CapabilityKind {
    Empty,
    Root,
    InitProgram,
    Endpoint,
}

struct CapabilitySlot {
    slot_id: u32
    owner_pid: u32
    kind: CapabilityKind
    rights: u32
    object_id: u32
}

func empty_slot() CapabilitySlot {
    return CapabilitySlot{ slot_id: 0, owner_pid: 0, kind: CapabilityKind.Empty, rights: 0, object_id: 0 }
}

func bootstrap_root_slot(owner_pid: u32) CapabilitySlot {
    return CapabilitySlot{ slot_id: 1, owner_pid: owner_pid, kind: CapabilityKind.Root, rights: 255, object_id: 1 }
}

func bootstrap_init_program_slot(owner_pid: u32) CapabilitySlot {
    return CapabilitySlot{ slot_id: 2, owner_pid: owner_pid, kind: CapabilityKind.InitProgram, rights: 7, object_id: 1 }
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