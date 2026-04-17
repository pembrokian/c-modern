import primitives
import service_effect
import syscall

const FILE_CAPACITY: usize = 4

// File operation codes forwarded by the shell to this service.
// These match the serial_protocol CMD_* bytes for the same letters so the
// shell can pass them through without a translation table.
const FILE_OP_CREATE: u8 = 67   // 'C'
const FILE_OP_WRITE: u8 = 87    // 'W'
const FILE_OP_READ: u8 = 82     // 'R'

struct FileSlot {
    name: u8
    used: bool
    len: u8
    data0: u8
    data1: u8
    data2: u8
    data3: u8
}

struct FileServiceState {
    pid: u32
    slot: u32
    slot0: FileSlot
    slot1: FileSlot
    slot2: FileSlot
    slot3: FileSlot
    count: usize
}

struct FileResult {
    state: FileServiceState
    effect: service_effect.Effect
}

func empty_file_slot() FileSlot {
    return FileSlot{ name: 0, used: false, len: 0, data0: 0, data1: 0, data2: 0, data3: 0 }
}

func file_init(pid: u32, slot: u32) FileServiceState {
    empty := empty_file_slot()
    return FileServiceState{ pid: pid, slot: slot, slot0: empty, slot1: empty, slot2: empty, slot3: empty, count: 0 }
}

func filewith(s: FileServiceState, slot0: FileSlot, slot1: FileSlot, slot2: FileSlot, slot3: FileSlot, count: usize) FileServiceState {
    return FileServiceState{ pid: s.pid, slot: s.slot, slot0: slot0, slot1: slot1, slot2: slot2, slot3: slot3, count: count }
}

func file_slot_at(s: FileServiceState, idx: usize) FileSlot {
    if idx == 0 {
        return s.slot0
    }
    if idx == 1 {
        return s.slot1
    }
    if idx == 2 {
        return s.slot2
    }
    return s.slot3
}

func file_with_slot(s: FileServiceState, idx: usize, slot: FileSlot) FileServiceState {
    if idx == 0 {
        return filewith(s, slot, s.slot1, s.slot2, s.slot3, s.count)
    }
    if idx == 1 {
        return filewith(s, s.slot0, slot, s.slot2, s.slot3, s.count)
    }
    if idx == 2 {
        return filewith(s, s.slot0, s.slot1, slot, s.slot3, s.count)
    }
    return filewith(s, s.slot0, s.slot1, s.slot2, slot, s.count)
}

func file_append_slot(s: FileServiceState, slot: FileSlot) FileServiceState {
    if s.count == 0 {
        return filewith(s, slot, s.slot1, s.slot2, s.slot3, 1)
    }
    if s.count == 1 {
        return filewith(s, s.slot0, slot, s.slot2, s.slot3, 2)
    }
    if s.count == 2 {
        return filewith(s, s.slot0, s.slot1, slot, s.slot3, 3)
    }
    return filewith(s, s.slot0, s.slot1, s.slot2, slot, 4)
}

// file_find returns the slot index for name, or FILE_CAPACITY if not found.
func file_find(s: FileServiceState, name: u8) usize {
    for i in 0..s.count {
        if file_slot_at(s, i).name == name {
            return i
        }
    }
    return FILE_CAPACITY
}

func file_create(s: FileServiceState, name: u8) FileResult {
    idx := file_find(s, name)
    if idx < FILE_CAPACITY {
        return FileResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.Ok, 0, primitives.zero_payload()) }
    }
    if s.count >= FILE_CAPACITY {
        return FileResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.Exhausted, 0, primitives.zero_payload()) }
    }
    created := FileSlot{ name: name, used: true, len: 0, data0: 0, data1: 0, data2: 0, data3: 0 }
    return FileResult{ state: file_append_slot(s, created), effect: service_effect.effect_reply(syscall.SyscallStatus.Ok, 0, primitives.zero_payload()) }
}

func file_write(s: FileServiceState, name: u8, val: u8) FileResult {
    idx := file_find(s, name)
    if idx >= FILE_CAPACITY {
        return FileResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.InvalidArgument, 0, primitives.zero_payload()) }
    }
    slot := file_slot_at(s, idx)
    if slot.len >= 4 {
        return FileResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.Exhausted, 0, primitives.zero_payload()) }
    }
    next_len := slot.len + 1
    written := slot
    switch slot.len {
    case 0:
        written = slot with { len: next_len, data0: val }
    case 1:
        written = slot with { len: next_len, data1: val }
    case 2:
        written = slot with { len: next_len, data2: val }
    default:
        written = slot with { len: next_len, data3: val }
    }
    return FileResult{ state: file_with_slot(s, idx, written), effect: service_effect.effect_reply(syscall.SyscallStatus.Ok, 0, primitives.zero_payload()) }
}

func file_read(s: FileServiceState, name: u8) FileResult {
    idx := file_find(s, name)
    if idx >= FILE_CAPACITY {
        return FileResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.InvalidArgument, 0, primitives.zero_payload()) }
    }
    slot := file_slot_at(s, idx)
    reply: [4]u8 = primitives.zero_payload()
    reply[0] = slot.data0
    reply[1] = slot.data1
    reply[2] = slot.data2
    reply[3] = slot.data3
    return FileResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.Ok, usize(slot.len), reply) }
}

func handle(s: FileServiceState, m: service_effect.Message) FileResult {
    if m.payload_len == 0 {
        return FileResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.Ok, s.count, primitives.zero_payload()) }
    }
    if m.payload_len < 2 {
        return FileResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.InvalidArgument, 0, primitives.zero_payload()) }
    }
    op: u8 = m.payload[0]
    name: u8 = m.payload[1]
    switch op {
    case FILE_OP_CREATE:
        return file_create(s, name)
    case FILE_OP_READ:
        return file_read(s, name)
    case FILE_OP_WRITE:
        if m.payload_len < 3 {
            return FileResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.InvalidArgument, 0, primitives.zero_payload()) }
        }
        return file_write(s, name, m.payload[2])
    default:
        return FileResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.InvalidArgument, 0, primitives.zero_payload()) }
    }
}

func filerecv(s: FileServiceState, obs: syscall.ReceiveObservation) FileResult {
    return handle(s, service_effect.from_receive_observation(obs))
}

func file_count(s: FileServiceState) usize {
    return s.count
}
