import fs
import primitives
import program_catalog
import service_effect
import syscall

const UPDATE_ARTIFACT_CAPACITY: usize = 4
const UPDATE_STORE_FORMAT_VERSION: u8 = 2
const UPDATE_STORE_ARTIFACT_SIZE: usize = 16
const UPDATE_STORE_LAUNCH_RECORD_SIZE: usize = 8
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

struct InstalledProgramSlot {
    present: bool
    version: u8
    len: usize
    data: [UPDATE_ARTIFACT_CAPACITY]u8
}

struct InstalledLaunchRecord {
    present: bool
    program: u8
    version: u8
    len: usize
    launcher_generation: u32
}

struct UpdateStoreServiceState {
    pid: u32
    slot: u32
    len: usize
    data: [UPDATE_ARTIFACT_CAPACITY]u8
    manifest: UpdateManifest
    installed: InstalledProgramSlot
    launched: InstalledLaunchRecord
}

struct UpdateStoreResult {
    state: UpdateStoreServiceState
    effect: service_effect.Effect
}

func update_manifest_init() UpdateManifest {
    return UpdateManifest{ present: false, version: 0, expected_len: 0 }
}

func update_installed_init() InstalledProgramSlot {
    return InstalledProgramSlot{ present: false, version: 0, len: 0, data: primitives.zero_payload() }
}

func update_launch_init() InstalledLaunchRecord {
    return InstalledLaunchRecord{ present: false, program: program_catalog.PROGRAM_ID_NONE, version: 0, len: 0, launcher_generation: 0 }
}

func update_store_init(pid: u32, slot: u32) UpdateStoreServiceState {
    return UpdateStoreServiceState{ pid: pid, slot: slot, len: 0, data: primitives.zero_payload(), manifest: update_manifest_init(), installed: update_installed_init(), launched: update_launch_init() }
}

func update_storewith(s: UpdateStoreServiceState, len: usize, data: [UPDATE_ARTIFACT_CAPACITY]u8, manifest: UpdateManifest, installed: InstalledProgramSlot, launched: InstalledLaunchRecord) UpdateStoreServiceState {
    return UpdateStoreServiceState{ pid: s.pid, slot: s.slot, len: len, data: data, manifest: manifest, installed: installed, launched: launched }
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

func update_installed_equal(left: InstalledProgramSlot, right: InstalledProgramSlot) bool {
    if left.present != right.present {
        return false
    }
    if left.version != right.version {
        return false
    }
    if left.len != right.len {
        return false
    }
    for i in 0..UPDATE_ARTIFACT_CAPACITY {
        if left.data[i] != right.data[i] {
            return false
        }
    }
    return true
}

func update_launch_equal(left: InstalledLaunchRecord, right: InstalledLaunchRecord) bool {
    if left.present != right.present {
        return false
    }
    if left.program != right.program {
        return false
    }
    if left.version != right.version {
        return false
    }
    if left.len != right.len {
        return false
    }
    if left.launcher_generation != right.launcher_generation {
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
    if !update_installed_equal(before.installed, after.installed) {
        return true
    }
    if !update_launch_equal(before.launched, after.launched) {
        return true
    }
    for i in 0..UPDATE_ARTIFACT_CAPACITY {
        if before.data[i] != after.data[i] {
            return true
        }
    }
    return false
}

func update_installed_present(s: UpdateStoreServiceState) bool {
    return s.installed.present
}

func update_installed_program_id(s: UpdateStoreServiceState) u8 {
    if !update_installed_present(s) {
        return program_catalog.PROGRAM_ID_NONE
    }
    return program_catalog.PROGRAM_ID_ISSUE_ROLLUP
}

func update_installed_version(s: UpdateStoreServiceState) u8 {
    return s.installed.version
}

func update_installed_len(s: UpdateStoreServiceState) usize {
    return s.installed.len
}

func update_installed_byte(s: UpdateStoreServiceState, idx: usize) u8 {
    if idx >= UPDATE_ARTIFACT_CAPACITY {
        return 0
    }
    return s.installed.data[idx]
}

func update_applied_present(s: UpdateStoreServiceState) bool {
    return update_installed_present(s)
}

func update_applied_version(s: UpdateStoreServiceState) u8 {
    return update_installed_version(s)
}

func update_applied_len(s: UpdateStoreServiceState) usize {
    return update_installed_len(s)
}

func update_applied_byte(s: UpdateStoreServiceState, idx: usize) u8 {
    return update_installed_byte(s, idx)
}

func update_launched_present(s: UpdateStoreServiceState) bool {
    return s.launched.present
}

func update_launched_program_id(s: UpdateStoreServiceState) u8 {
    return s.launched.program
}

func update_launched_generation(s: UpdateStoreServiceState) u32 {
    return s.launched.launcher_generation
}

func update_launch_matches_installed_program(s: UpdateStoreServiceState, program: u8) bool {
    if !s.launched.present {
        return false
    }
    if update_installed_program_id(s) != program {
        return false
    }
    if s.launched.program != program {
        return false
    }
    if s.launched.version != update_installed_version(s) {
        return false
    }
    if s.launched.len != update_installed_len(s) {
        return false
    }
    return true
}

func update_record_launch(s: UpdateStoreServiceState, program: u8, launcher_generation: u32) UpdateStoreServiceState {
    launched := InstalledLaunchRecord{
        present: true,
        program: program,
        version: update_installed_version(s),
        len: update_installed_len(s),
        launcher_generation: launcher_generation
    }
    return update_storewith(s, s.len, s.data, s.manifest, s.installed, launched)
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
    if s.installed.present {
        bytes[9] = 1
    } else {
        bytes[9] = 0
    }
    bytes[10] = s.installed.version
    bytes[11] = u8(s.installed.len)
    for i in 0..UPDATE_ARTIFACT_CAPACITY {
        bytes[12 + i] = s.installed.data[i]
    }
    return bytes
}

func update_store_launch_bytes(s: UpdateStoreServiceState) [UPDATE_STORE_LAUNCH_RECORD_SIZE]u8 {
    bytes: [UPDATE_STORE_LAUNCH_RECORD_SIZE]u8
    if s.launched.present {
        bytes[0] = 1
    } else {
        bytes[0] = 0
    }
    bytes[1] = s.launched.program
    bytes[2] = s.launched.version
    bytes[3] = u8(s.launched.len)
    bytes[4] = u8(s.launched.launcher_generation >> 24)
    bytes[5] = u8(s.launched.launcher_generation >> 16)
    bytes[6] = u8(s.launched.launcher_generation >> 8)
    bytes[7] = u8(s.launched.launcher_generation)
    return bytes
}

func update_store_persist(s: UpdateStoreServiceState) bool {
    bytes := update_store_artifact_bytes(s)
    if !fs.write_all("mc_update_store_service.bin", (Slice<u8>)(bytes)) {
        return false
    }
    launch_bytes := update_store_launch_bytes(s)
    return fs.write_all("mc_update_store_launch.bin", (Slice<u8>)(launch_bytes))
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
    if usize(bytes[1]) > UPDATE_ARTIFACT_CAPACITY || usize(bytes[4]) > UPDATE_ARTIFACT_CAPACITY || usize(bytes[11]) > UPDATE_ARTIFACT_CAPACITY {
        return state
    }

    data: [UPDATE_ARTIFACT_CAPACITY]u8 = primitives.zero_payload()
    for i in 0..UPDATE_ARTIFACT_CAPACITY {
        data[i] = bytes[5 + i]
    }
    manifest := UpdateManifest{ present: bytes[2] == 1, version: bytes[3], expected_len: usize(bytes[4]) }
    installed_data: [UPDATE_ARTIFACT_CAPACITY]u8 = primitives.zero_payload()
    for i in 0..UPDATE_ARTIFACT_CAPACITY {
        installed_data[i] = bytes[12 + i]
    }
    installed := InstalledProgramSlot{ present: bytes[9] == 1, version: bytes[10], len: usize(bytes[11]), data: installed_data }
    launch_bytes: [UPDATE_STORE_LAUNCH_RECORD_SIZE]u8
    launched := update_launch_init()
    if fs.read_exact("mc_update_store_launch.bin", (Slice<u8>)(launch_bytes)) {
        launched = InstalledLaunchRecord{
            present: launch_bytes[0] == 1,
            program: launch_bytes[1],
            version: launch_bytes[2],
            len: usize(launch_bytes[3]),
            launcher_generation: (u32(launch_bytes[4]) << 24) + (u32(launch_bytes[5]) << 16) + (u32(launch_bytes[6]) << 8) + u32(launch_bytes[7])
        }
    }
    return UpdateStoreServiceState{ pid: pid, slot: slot, len: usize(bytes[1]), data: data, manifest: manifest, installed: installed, launched: launched }
}

func update_stage(s: UpdateStoreServiceState, value: u8) UpdateStoreResult {
    if s.len >= UPDATE_ARTIFACT_CAPACITY {
        return UpdateStoreResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.Exhausted, 0, primitives.zero_payload()) }
    }
    data := s.data
    data[s.len] = value
    return UpdateStoreResult{ state: update_storewith(s, s.len + 1, data, s.manifest, s.installed, s.launched), effect: service_effect.effect_reply(syscall.SyscallStatus.Ok, 0, primitives.zero_payload()) }
}

func update_record_manifest(s: UpdateStoreServiceState, version: u8, expected_len: u8) UpdateStoreResult {
    if usize(expected_len) > UPDATE_ARTIFACT_CAPACITY {
        return UpdateStoreResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.InvalidArgument, 0, primitives.zero_payload()) }
    }
    manifest := UpdateManifest{ present: true, version: version, expected_len: usize(expected_len) }
    return UpdateStoreResult{ state: update_storewith(s, s.len, s.data, manifest, s.installed, s.launched), effect: service_effect.effect_reply(syscall.SyscallStatus.Ok, 0, primitives.zero_payload()) }
}

func update_apply(s: UpdateStoreServiceState) UpdateStoreResult {
    if update_manifest_classification(s) != UPDATE_CLASS_READY {
        return UpdateStoreResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.InvalidArgument, 0, primitives.zero_payload()) }
    }
    installed := InstalledProgramSlot{ present: true, version: s.manifest.version, len: s.len, data: s.data }
    if update_installed_equal(s.installed, installed) {
        return UpdateStoreResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.Ok, 0, primitives.zero_payload()) }
    }
    return UpdateStoreResult{ state: update_storewith(s, s.len, s.data, s.manifest, installed, s.launched), effect: service_effect.effect_reply(syscall.SyscallStatus.Ok, 0, primitives.zero_payload()) }
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
    cleared := update_store_init(s.pid, s.slot)
    return UpdateStoreResult{ state: update_storewith(cleared, cleared.len, cleared.data, cleared.manifest, s.installed, s.launched), effect: service_effect.effect_reply(syscall.SyscallStatus.Ok, 0, primitives.zero_payload()) }
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