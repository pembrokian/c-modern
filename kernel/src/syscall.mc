enum SyscallId {
    None,
    Send,
    Receive,
    Spawn,
}

struct SyscallGate {
    open: u32
    last_id: SyscallId
}

func gate_closed() SyscallGate {
    return SyscallGate{ open: 0, last_id: SyscallId.None }
}

func id_score(id: SyscallId) i32 {
    switch id {
    case SyscallId.None:
        return 1
    case SyscallId.Send:
        return 2
    case SyscallId.Receive:
        return 4
    case SyscallId.Spawn:
        return 8
    default:
        return 0
    }
    return 0
}