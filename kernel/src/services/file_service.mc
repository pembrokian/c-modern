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

// Each file holds a 1-byte name token, one stored data byte, and a written
// flag (0 = empty, 1 = data present).  Retained-only: no durable substrate.
struct FileServiceState {
    pid: u32
    slot: u32
    names: [FILE_CAPACITY]u8
    data: [FILE_CAPACITY]u8
    lens: [FILE_CAPACITY]u8
    count: usize
}

struct FileResult {
    state: FileServiceState
    effect: service_effect.Effect
}

func file_init(pid: u32, slot: u32) FileServiceState {
    return FileServiceState{ pid: pid, slot: slot, names: primitives.zero_payload(), data: primitives.zero_payload(), lens: primitives.zero_payload(), count: 0 }
}

func filewith(s: FileServiceState, names: [FILE_CAPACITY]u8, data: [FILE_CAPACITY]u8, lens: [FILE_CAPACITY]u8, count: usize) FileServiceState {
    return FileServiceState{ pid: s.pid, slot: s.slot, names: names, data: data, lens: lens, count: count }
}

// file_find returns the slot index for name, or FILE_CAPACITY if not found.
func file_find(s: FileServiceState, name: u8) usize {
    for i in 0..s.count {
        if s.names[i] == name {
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
    next_names: [FILE_CAPACITY]u8 = s.names
    next_names[s.count] = name
    return FileResult{ state: filewith(s, next_names, s.data, s.lens, s.count + 1), effect: service_effect.effect_reply(syscall.SyscallStatus.Ok, 0, primitives.zero_payload()) }
}

func file_write(s: FileServiceState, name: u8, val: u8) FileResult {
    idx := file_find(s, name)
    if idx >= FILE_CAPACITY {
        return FileResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.InvalidArgument, 0, primitives.zero_payload()) }
    }
    if s.lens[idx] >= 1 {
        return FileResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.Exhausted, 0, primitives.zero_payload()) }
    }
    next_data: [FILE_CAPACITY]u8 = s.data
    next_lens: [FILE_CAPACITY]u8 = s.lens
    next_data[idx] = val
    next_lens[idx] = 1
    return FileResult{ state: filewith(s, s.names, next_data, next_lens, s.count), effect: service_effect.effect_reply(syscall.SyscallStatus.Ok, 0, primitives.zero_payload()) }
}

func file_read(s: FileServiceState, name: u8) FileResult {
    idx := file_find(s, name)
    if idx >= FILE_CAPACITY {
        return FileResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.InvalidArgument, 0, primitives.zero_payload()) }
    }
    reply: [4]u8 = primitives.zero_payload()
    reply[0] = s.data[idx]
    return FileResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.Ok, usize(s.lens[idx]), reply) }
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
