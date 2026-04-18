import fs
import primitives
import service_effect
import syscall

const UPDATE_ARTIFACT_CAPACITY: usize = 4
const UPDATE_STORE_FORMAT_VERSION: u8 = 1
const UPDATE_STORE_ARTIFACT_SIZE: usize = 9
const UPDATE_OP_STAGE: u8 = 65
const UPDATE_OP_CLEAR: u8 = 67
const UPDATE_OP_QUERY: u8 = 81
const UPDATE_OP_MANIFEST: u8 = 77

const UPDATE_CLASS_EMPTY: u8 = 0
const UPDATE_CLASS_ARTIFACT_ONLY: u8 = 1
const UPDATE_CLASS_MANIFEST_ONLY: u8 = 2
const UPDATE_CLASS_READY: u8 = 3
const UPDATE_CLASS_MISMATCH: u8 = 4

struct UpdateManifest {
    present: bool
    version: u8
    expected_len: usize
}

struct UpdateStoreServiceState {
    pid: u32
    slot: u32
    len: usize
    data: [UPDATE_ARTIFACT_CAPACITY]u8
    manifest: UpdateManifest
}

struct UpdateStoreResult {
    state: UpdateStoreServiceState
    effect: service_effect.Effect
}

func update_manifest_init() UpdateManifest {
    return UpdateManifest{ present: false, version: 0, expected_len: 0 }
}

func update_store_init(pid: u32, slot: u32) UpdateStoreServiceState {
    return UpdateStoreServiceState{ pid: pid, slot: slot, len: 0, data: primitives.zero_payload(), manifest: update_manifest_init() }
}

func update_storewith(s: UpdateStoreServiceState, len: usize, data: [UPDATE_ARTIFACT_CAPACITY]u8, manifest: UpdateManifest) UpdateStoreServiceState {
    return UpdateStoreServiceState{ pid: s.pid, slot: s.slot, len: len, data: data, manifest: manifest }
}

func update_manifest_equal(left: UpdateManifest, right: UpdateManifest) bool {
    if left.present != right.present {
        return false
    }
    if left.version != right.version {
        return false
    }
    if left.expected_len != right.expected_len {
        return false
    }
    return true
}

func update_store_changed(before: UpdateStoreServiceState, after: UpdateStoreServiceState) bool {
    if before.len != after.len {
        return true
    }
    if !update_manifest_equal(before.manifest, after.manifest) {
        return true
    }
    for i in 0..UPDATE_ARTIFACT_CAPACITY {
        if before.data[i] != after.data[i] {
            return true
        }
    }
    return false
}

func update_manifest_classification(s: UpdateStoreServiceState) u8 {
    if s.len == 0 {
        if s.manifest.present {
            return UPDATE_CLASS_MANIFEST_ONLY
        }
        return UPDATE_CLASS_EMPTY
    }
    if !s.manifest.present {
        return UPDATE_CLASS_ARTIFACT_ONLY
    }
    if s.manifest.expected_len == s.len {
        return UPDATE_CLASS_READY
    }
    return UPDATE_CLASS_MISMATCH
}

func update_artifact_len(s: UpdateStoreServiceState) usize {
    return s.len
}

func update_store_artifact_bytes(s: UpdateStoreServiceState) [UPDATE_STORE_ARTIFACT_SIZE]u8 {
    bytes: [UPDATE_STORE_ARTIFACT_SIZE]u8
    bytes[0] = UPDATE_STORE_FORMAT_VERSION
    bytes[1] = u8(s.len)
    if s.manifest.present {
        bytes[2] = 1
    } else {
        bytes[2] = 0
    }
    bytes[3] = s.manifest.version
    bytes[4] = u8(s.manifest.expected_len)
    for i in 0..UPDATE_ARTIFACT_CAPACITY {
        bytes[5 + i] = s.data[i]
    }
    return bytes
}

func update_store_persist(s: UpdateStoreServiceState) bool {
    bytes := update_store_artifact_bytes(s)
    return fs.write_all("mc_update_store_service.bin", (Slice<u8>)(bytes))
}

func update_store_load(pid: u32, slot: u32) UpdateStoreServiceState {
    state := update_store_init(pid, slot)
    bytes: [UPDATE_STORE_ARTIFACT_SIZE]u8
    if !fs.read_exact("mc_update_store_service.bin", (Slice<u8>)(bytes)) {
        return state
    }

    if bytes[0] != UPDATE_STORE_FORMAT_VERSION {
        return state
    }
    if usize(bytes[1]) > UPDATE_ARTIFACT_CAPACITY || usize(bytes[4]) > UPDATE_ARTIFACT_CAPACITY {
        return state
    }

    data: [UPDATE_ARTIFACT_CAPACITY]u8 = primitives.zero_payload()
    for i in 0..UPDATE_ARTIFACT_CAPACITY {
        data[i] = bytes[5 + i]
    }
    manifest := UpdateManifest{ present: bytes[2] == 1, version: bytes[3], expected_len: usize(bytes[4]) }
    return UpdateStoreServiceState{ pid: pid, slot: slot, len: usize(bytes[1]), data: data, manifest: manifest }
}

func update_stage(s: UpdateStoreServiceState, value: u8) UpdateStoreResult {
    if s.len >= UPDATE_ARTIFACT_CAPACITY {
        return UpdateStoreResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.Exhausted, 0, primitives.zero_payload()) }
    }
    data := s.data
    data[s.len] = value
    return UpdateStoreResult{ state: update_storewith(s, s.len + 1, data, s.manifest), effect: service_effect.effect_reply(syscall.SyscallStatus.Ok, 0, primitives.zero_payload()) }
}

func update_record_manifest(s: UpdateStoreServiceState, version: u8, expected_len: u8) UpdateStoreResult {
    if usize(expected_len) > UPDATE_ARTIFACT_CAPACITY {
        return UpdateStoreResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.InvalidArgument, 0, primitives.zero_payload()) }
    }
    manifest := UpdateManifest{ present: true, version: version, expected_len: usize(expected_len) }
    return UpdateStoreResult{ state: update_storewith(s, s.len, s.data, manifest), effect: service_effect.effect_reply(syscall.SyscallStatus.Ok, 0, primitives.zero_payload()) }
}

func update_query(s: UpdateStoreServiceState) UpdateStoreResult {
    payload := primitives.zero_payload()
    payload[0] = update_manifest_classification(s)
    payload[1] = s.manifest.version
    payload[2] = u8(s.len)
    payload[3] = u8(s.manifest.expected_len)
    return UpdateStoreResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.Ok, 4, payload) }
}

func update_clear(s: UpdateStoreServiceState) UpdateStoreResult {
    return UpdateStoreResult{ state: update_store_init(s.pid, s.slot), effect: service_effect.effect_reply(syscall.SyscallStatus.Ok, 0, primitives.zero_payload()) }
}

func handle(s: UpdateStoreServiceState, m: service_effect.Message) UpdateStoreResult {
    if m.payload_len < 1 {
        return UpdateStoreResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.InvalidArgument, 0, primitives.zero_payload()) }
    }

    switch m.payload[0] {
    case UPDATE_OP_STAGE:
        if m.payload_len < 2 {
            return UpdateStoreResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.InvalidArgument, 0, primitives.zero_payload()) }
        }
        return update_stage(s, m.payload[1])
    case UPDATE_OP_MANIFEST:
        if m.payload_len < 3 {
            return UpdateStoreResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.InvalidArgument, 0, primitives.zero_payload()) }
        }
        return update_record_manifest(s, m.payload[1], m.payload[2])
    case UPDATE_OP_QUERY:
        return update_query(s)
    case UPDATE_OP_CLEAR:
        return update_clear(s)
    default:
        return UpdateStoreResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.InvalidArgument, 0, primitives.zero_payload()) }
    }
}