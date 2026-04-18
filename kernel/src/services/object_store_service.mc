import fs
import primitives
import service_effect
import syscall

const OBJECT_STORE_CAPACITY: usize = 4
const OBJECT_STORE_FORMAT_VERSION: u8 = 2
const OBJECT_STORE_ARTIFACT_SIZE: usize = 17
const OBJECT_OP_CREATE: u8 = 67
const OBJECT_OP_READ: u8 = 82
const OBJECT_OP_REPLACE: u8 = 87
const OBJECT_UPDATE_CONFLICT: u8 = 67

struct ObjectSlot {
    name: u8
    used: bool
    value: u8
    version: u8
}

struct ObjectStoreServiceState {
    pid: u32
    slot: u32
    slot0: ObjectSlot
    slot1: ObjectSlot
    slot2: ObjectSlot
    slot3: ObjectSlot
    count: usize
}

struct ObjectStoreResult {
    state: ObjectStoreServiceState
    effect: service_effect.Effect
}

func empty_object_slot() ObjectSlot {
    return ObjectSlot{ name: 0, used: false, value: 0, version: 0 }
}

func object_store_init(pid: u32, slot: u32) ObjectStoreServiceState {
    empty := empty_object_slot()
    return ObjectStoreServiceState{ pid: pid, slot: slot, slot0: empty, slot1: empty, slot2: empty, slot3: empty, count: 0 }
}

func object_storewith(s: ObjectStoreServiceState, slot0: ObjectSlot, slot1: ObjectSlot, slot2: ObjectSlot, slot3: ObjectSlot, count: usize) ObjectStoreServiceState {
    return ObjectStoreServiceState{ pid: s.pid, slot: s.slot, slot0: slot0, slot1: slot1, slot2: slot2, slot3: slot3, count: count }
}

func object_slot_at(s: ObjectStoreServiceState, idx: usize) ObjectSlot {
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

func object_with_slot(s: ObjectStoreServiceState, idx: usize, slot: ObjectSlot) ObjectStoreServiceState {
    if idx == 0 {
        return object_storewith(s, slot, s.slot1, s.slot2, s.slot3, s.count)
    }
    if idx == 1 {
        return object_storewith(s, s.slot0, slot, s.slot2, s.slot3, s.count)
    }
    if idx == 2 {
        return object_storewith(s, s.slot0, s.slot1, slot, s.slot3, s.count)
    }
    return object_storewith(s, s.slot0, s.slot1, s.slot2, slot, s.count)
}

func object_append_slot(s: ObjectStoreServiceState, slot: ObjectSlot) ObjectStoreServiceState {
    if s.count == 0 {
        return object_storewith(s, slot, s.slot1, s.slot2, s.slot3, 1)
    }
    if s.count == 1 {
        return object_storewith(s, s.slot0, slot, s.slot2, s.slot3, 2)
    }
    if s.count == 2 {
        return object_storewith(s, s.slot0, s.slot1, slot, s.slot3, 3)
    }
    return object_storewith(s, s.slot0, s.slot1, s.slot2, slot, 4)
}

func object_find(s: ObjectStoreServiceState, name: u8) usize {
    for i in 0..s.count {
        if object_slot_at(s, i).name == name {
            return i
        }
    }
    return OBJECT_STORE_CAPACITY
}

func object_count(s: ObjectStoreServiceState) usize {
    return s.count
}

func object_slot_equal(left: ObjectSlot, right: ObjectSlot) bool {
    if left.name != right.name {
        return false
    }
    if left.used != right.used {
        return false
    }
    if left.value != right.value {
        return false
    }
    if left.version != right.version {
        return false
    }
    return true
}

func object_update_conflict() service_effect.Effect {
    payload := primitives.zero_payload()
    payload[0] = OBJECT_UPDATE_CONFLICT
    return service_effect.effect_reply(syscall.SyscallStatus.InvalidArgument, 1, payload)
}

func object_next_version(version: u8) u8 {
    if version == 255 {
        return 1
    }
    return version + 1
}

func object_current_version(s: ObjectStoreServiceState, name: u8) u8 {
    idx := object_find(s, name)
    if idx >= OBJECT_STORE_CAPACITY {
        return 0
    }
    return object_slot_at(s, idx).version
}

func object_store_changed(before: ObjectStoreServiceState, after: ObjectStoreServiceState) bool {
    if before.count != after.count {
        return true
    }
    if !object_slot_equal(before.slot0, after.slot0) {
        return true
    }
    if !object_slot_equal(before.slot1, after.slot1) {
        return true
    }
    if !object_slot_equal(before.slot2, after.slot2) {
        return true
    }
    if !object_slot_equal(before.slot3, after.slot3) {
        return true
    }
    return false
}

func object_store_artifact_bytes(s: ObjectStoreServiceState) [OBJECT_STORE_ARTIFACT_SIZE]u8 {
    bytes: [OBJECT_STORE_ARTIFACT_SIZE]u8
    bytes[0] = OBJECT_STORE_FORMAT_VERSION
    slots: [4]ObjectSlot
    slots[0] = s.slot0
    slots[1] = s.slot1
    slots[2] = s.slot2
    slots[3] = s.slot3
    for i in 0..OBJECT_STORE_CAPACITY {
        base := 1 + i * 4
        if slots[i].used {
            bytes[base] = 1
        } else {
            bytes[base] = 0
        }
        bytes[base + 1] = slots[i].name
        bytes[base + 2] = slots[i].value
        bytes[base + 3] = slots[i].version
    }
    return bytes
}

func object_store_persist(s: ObjectStoreServiceState) bool {
    bytes := object_store_artifact_bytes(s)
    return fs.write_all("mc_object_store_service.bin", (Slice<u8>)(bytes))
}

func object_store_load(pid: u32, slot: u32) ObjectStoreServiceState {
    state := object_store_init(pid, slot)
    bytes: [OBJECT_STORE_ARTIFACT_SIZE]u8
    if !fs.read_exact("mc_object_store_service.bin", (Slice<u8>)(bytes)) {
        return state
    }

    if bytes[0] != OBJECT_STORE_FORMAT_VERSION {
        return state
    }

    next0 := empty_object_slot()
    next1 := empty_object_slot()
    next2 := empty_object_slot()
    next3 := empty_object_slot()
    count := 0
    for i in 0..OBJECT_STORE_CAPACITY {
        base := 1 + i * 4
        used := bytes[base] == 1
        current := ObjectSlot{ name: bytes[base + 1], used: used, value: bytes[base + 2], version: bytes[base + 3] }
        if used {
            count = count + 1
        }
        if i == 0 {
            next0 = current
        }
        if i == 1 {
            next1 = current
        }
        if i == 2 {
            next2 = current
        }
        if i == 3 {
            next3 = current
        }
    }
    return ObjectStoreServiceState{ pid: pid, slot: slot, slot0: next0, slot1: next1, slot2: next2, slot3: next3, count: count }
}

func object_create(s: ObjectStoreServiceState, name: u8, value: u8) ObjectStoreResult {
    idx := object_find(s, name)
    if idx < OBJECT_STORE_CAPACITY {
        return ObjectStoreResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.InvalidArgument, 0, primitives.zero_payload()) }
    }
    if s.count >= OBJECT_STORE_CAPACITY {
        return ObjectStoreResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.Exhausted, 0, primitives.zero_payload()) }
    }
    created := ObjectSlot{ name: name, used: true, value: value, version: 1 }
    return ObjectStoreResult{ state: object_append_slot(s, created), effect: service_effect.effect_reply(syscall.SyscallStatus.Ok, 0, primitives.zero_payload()) }
}

func object_read(s: ObjectStoreServiceState, name: u8) ObjectStoreResult {
    idx := object_find(s, name)
    if idx >= OBJECT_STORE_CAPACITY {
        return ObjectStoreResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.InvalidArgument, 0, primitives.zero_payload()) }
    }
    payload := primitives.zero_payload()
    payload[0] = object_slot_at(s, idx).value
    return ObjectStoreResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.Ok, 1, payload) }
}

func object_replace(s: ObjectStoreServiceState, name: u8, value: u8) ObjectStoreResult {
    idx := object_find(s, name)
    if idx >= OBJECT_STORE_CAPACITY {
        return ObjectStoreResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.InvalidArgument, 0, primitives.zero_payload()) }
    }
    current := object_slot_at(s, idx)
    if current.value == value {
        return ObjectStoreResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.Ok, 0, primitives.zero_payload()) }
    }
    replaced := current with { value: value, version: object_next_version(current.version) }
    return ObjectStoreResult{ state: object_with_slot(s, idx, replaced), effect: service_effect.effect_reply(syscall.SyscallStatus.Ok, 0, primitives.zero_payload()) }
}

func object_update(s: ObjectStoreServiceState, name: u8, value: u8) ObjectStoreResult {
    idx := object_find(s, name)
    if idx >= OBJECT_STORE_CAPACITY {
        return ObjectStoreResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.InvalidArgument, 0, primitives.zero_payload()) }
    }
    current := object_slot_at(s, idx)
    if current.value == value {
        return ObjectStoreResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.Ok, 0, primitives.zero_payload()) }
    }
    return object_replace(s, name, value)
}

func object_update_if_version(s: ObjectStoreServiceState, name: u8, value: u8, version: u8) ObjectStoreResult {
    idx := object_find(s, name)
    if idx >= OBJECT_STORE_CAPACITY {
        return ObjectStoreResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.InvalidArgument, 0, primitives.zero_payload()) }
    }
    current := object_slot_at(s, idx)
    if current.version != version {
        return ObjectStoreResult{ state: s, effect: object_update_conflict() }
    }
    return object_update(s, name, value)
}

func handle(s: ObjectStoreServiceState, m: service_effect.Message) ObjectStoreResult {
    if m.payload_len < 2 {
        return ObjectStoreResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.InvalidArgument, 0, primitives.zero_payload()) }
    }

    op := m.payload[0]
    name := m.payload[1]
    switch op {
    case OBJECT_OP_CREATE:
        if m.payload_len < 3 {
            return ObjectStoreResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.InvalidArgument, 0, primitives.zero_payload()) }
        }
        return object_create(s, name, m.payload[2])
    case OBJECT_OP_READ:
        return object_read(s, name)
    case OBJECT_OP_REPLACE:
        if m.payload_len < 3 {
            return ObjectStoreResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.InvalidArgument, 0, primitives.zero_payload()) }
        }
        return object_replace(s, name, m.payload[2])
    default:
        return ObjectStoreResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.InvalidArgument, 0, primitives.zero_payload()) }
    }
}