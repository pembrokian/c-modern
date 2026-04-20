import fs
import primitives
import service_effect
import syscall

const JOURNAL_RECORD_CAPACITY: usize = 4
const JOURNAL_FORMAT_VERSION: u8 = 1
const JOURNAL_ARTIFACT_SIZE: usize = 11
const JOURNAL_OP_APPEND: u8 = 65
const JOURNAL_OP_CLEAR: u8 = 67
const JOURNAL_OP_REPLAY: u8 = 82
const JOURNAL_LANE_A: u8 = 65
const JOURNAL_LANE_B: u8 = 66
const JOURNAL_ARTIFACT_PATH: str = "mc_journal_service.bin"

struct JournalLane {
    name: u8
    len: usize
    data: [JOURNAL_RECORD_CAPACITY]u8
}

struct JournalServiceState {
    pid: u32
    slot: u32
    lane0: JournalLane
    lane1: JournalLane
}

struct JournalResult {
    state: JournalServiceState
    effect: service_effect.Effect
}

func lane_init(name: u8) JournalLane {
    return JournalLane{ name: name, len: 0, data: primitives.zero_payload() }
}

func journal_init(pid: u32, slot: u32) JournalServiceState {
    return JournalServiceState{ pid: pid, slot: slot, lane0: lane_init(JOURNAL_LANE_A), lane1: lane_init(JOURNAL_LANE_B) }
}

func journalwith(s: JournalServiceState, lane0: JournalLane, lane1: JournalLane) JournalServiceState {
    return JournalServiceState{ pid: s.pid, slot: s.slot, lane0: lane0, lane1: lane1 }
}

func lane_valid(name: u8) bool {
    return name == JOURNAL_LANE_A || name == JOURNAL_LANE_B
}

func lane_at(s: JournalServiceState, name: u8) JournalLane {
    if name == JOURNAL_LANE_A {
        return s.lane0
    }
    return s.lane1
}

func journal_with_lane(s: JournalServiceState, name: u8, lane: JournalLane) JournalServiceState {
    if name == JOURNAL_LANE_A {
        return journalwith(s, lane, s.lane1)
    }
    return journalwith(s, s.lane0, lane)
}

func journal_write_lane(s: JournalServiceState, name: u8, len: usize, data: [JOURNAL_RECORD_CAPACITY]u8) JournalServiceState {
    if !lane_valid(name) || len > JOURNAL_RECORD_CAPACITY {
        return s
    }
    return journal_with_lane(s, name, JournalLane{ name: name, len: len, data: data })
}

func lane_equal(left: JournalLane, right: JournalLane) bool {
    if left.name != right.name || left.len != right.len {
        return false
    }
    for i in 0..JOURNAL_RECORD_CAPACITY {
        if left.data[i] != right.data[i] {
            return false
        }
    }
    return true
}

func journal_changed(before: JournalServiceState, after: JournalServiceState) bool {
    if !lane_equal(before.lane0, after.lane0) {
        return true
    }
    if !lane_equal(before.lane1, after.lane1) {
        return true
    }
    return false
}

func journal_count(s: JournalServiceState) usize {
    return s.lane0.len + s.lane1.len
}

func journal_artifact_bytes(s: JournalServiceState) [JOURNAL_ARTIFACT_SIZE]u8 {
    bytes: [JOURNAL_ARTIFACT_SIZE]u8
    bytes[0] = JOURNAL_FORMAT_VERSION
    bytes[1] = u8(s.lane0.len)
    for i in 0..JOURNAL_RECORD_CAPACITY {
        bytes[2 + i] = s.lane0.data[i]
    }
    bytes[6] = u8(s.lane1.len)
    for i in 0..JOURNAL_RECORD_CAPACITY {
        bytes[7 + i] = s.lane1.data[i]
    }
    return bytes
}

func journal_persist(s: JournalServiceState) bool {
    bytes := journal_artifact_bytes(s)
    return fs.write_all(JOURNAL_ARTIFACT_PATH, (Slice<u8>)(bytes))
}

func journal_load(pid: u32, slot: u32) JournalServiceState {
    state := journal_init(pid, slot)
    bytes: [JOURNAL_ARTIFACT_SIZE]u8
    if !fs.read_exact(JOURNAL_ARTIFACT_PATH, (Slice<u8>)(bytes)) {
        return state
    }

    if bytes[0] != JOURNAL_FORMAT_VERSION {
        return state
    }
    if usize(bytes[1]) > JOURNAL_RECORD_CAPACITY || usize(bytes[6]) > JOURNAL_RECORD_CAPACITY {
        return state
    }

    lane0data: [JOURNAL_RECORD_CAPACITY]u8 = primitives.zero_payload()
    lane1data: [JOURNAL_RECORD_CAPACITY]u8 = primitives.zero_payload()
    for i in 0..JOURNAL_RECORD_CAPACITY {
        lane0data[i] = bytes[2 + i]
        lane1data[i] = bytes[7 + i]
    }

    return JournalServiceState{
        pid: pid,
        slot: slot,
        lane0: JournalLane{ name: JOURNAL_LANE_A, len: usize(bytes[1]), data: lane0data },
        lane1: JournalLane{ name: JOURNAL_LANE_B, len: usize(bytes[6]), data: lane1data }
    }
}

func journal_append(s: JournalServiceState, name: u8, value: u8) JournalResult {
    lane := lane_at(s, name)
    if lane.len >= JOURNAL_RECORD_CAPACITY {
        return JournalResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.Exhausted, 0, primitives.zero_payload()) }
    }
    data := lane.data
    data[lane.len] = value
    next := JournalLane{ name: lane.name, len: lane.len + 1, data: data }
    return JournalResult{ state: journal_with_lane(s, name, next), effect: service_effect.effect_reply(syscall.SyscallStatus.Ok, 0, primitives.zero_payload()) }
}

func journal_replay(s: JournalServiceState, name: u8) JournalResult {
    lane := lane_at(s, name)
    payload: [JOURNAL_RECORD_CAPACITY]u8 = primitives.zero_payload()
    for i in 0..lane.len {
        payload[i] = lane.data[i]
    }
    return JournalResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.Ok, lane.len, payload) }
}

func journal_clear(s: JournalServiceState, name: u8) JournalResult {
    return JournalResult{ state: journal_with_lane(s, name, lane_init(name)), effect: service_effect.effect_reply(syscall.SyscallStatus.Ok, 0, primitives.zero_payload()) }
}

func handle(s: JournalServiceState, m: service_effect.Message) JournalResult {
    if m.payload_len < 2 {
        return JournalResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.InvalidArgument, 0, primitives.zero_payload()) }
    }

    name := m.payload[1]
    if !lane_valid(name) {
        return JournalResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.InvalidArgument, 0, primitives.zero_payload()) }
    }

    switch m.payload[0] {
    case JOURNAL_OP_APPEND:
        if m.payload_len < 3 {
            return JournalResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.InvalidArgument, 0, primitives.zero_payload()) }
        }
        return journal_append(s, name, m.payload[2])
    case JOURNAL_OP_REPLAY:
        return journal_replay(s, name)
    case JOURNAL_OP_CLEAR:
        return journal_clear(s, name)
    default:
        return JournalResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.InvalidArgument, 0, primitives.zero_payload()) }
    }
}