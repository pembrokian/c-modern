// Compact service-state inspection contract for fixed-target shell queries.

import identity_taxonomy
import serial_protocol

const STATE_CLASS_FIXED: u8 = 0
const STATE_CLASS_ORDINARY: u8 = 1
const STATE_CLASS_RETAINED: u8 = 2
const STATE_CLASS_LANE: u8 = 3
const STATE_CLASS_DURABLE: u8 = 4

const STATE_MODE_NONE: u8 = 0
const STATE_MODE_RESET: u8 = 1
const STATE_MODE_RELOAD: u8 = 2

const STATE_PARTICIPATION_NONE: u8 = 0
const STATE_PARTICIPATION_SERVICE: u8 = 1
const STATE_PARTICIPATION_LANE: u8 = 2

const STATE_POLICY_NONE: u8 = 0
const STATE_POLICY_CLEAR: u8 = 1
const STATE_POLICY_KEEP: u8 = 2

const STATE_DURABILITY_NONE: u8 = 0
const STATE_DURABILITY_RETAINED: u8 = 1
const STATE_DURABILITY_DURABLE: u8 = 2

const STATE_DURABLE_REQ_NONE: u8 = 0
const STATE_DURABLE_REQ_PERSISTENCE: u8 = 1
const STATE_DURABLE_REQ_NAMING: u8 = 2
const STATE_DURABLE_REQ_RECOVERY: u8 = 4

func state_class(target: u8) u8 {
    if identity_taxonomy.identity_target_is_lane(target) {
        return STATE_CLASS_LANE
    }
    switch target {
    case serial_protocol.TARGET_LOG:
        return STATE_CLASS_RETAINED
    case serial_protocol.TARGET_KV:
        return STATE_CLASS_RETAINED
    case serial_protocol.TARGET_QUEUE:
        return STATE_CLASS_RETAINED
    case serial_protocol.TARGET_FILE:
        return STATE_CLASS_RETAINED
    case serial_protocol.TARGET_TIMER:
        return STATE_CLASS_RETAINED
    case serial_protocol.TARGET_JOURNAL:
        return STATE_CLASS_DURABLE
    case serial_protocol.TARGET_TASK:
        return STATE_CLASS_ORDINARY
    case serial_protocol.TARGET_ECHO:
        return STATE_CLASS_ORDINARY
    case serial_protocol.TARGET_TRANSFER:
        return STATE_CLASS_ORDINARY
    case serial_protocol.TARGET_TICKET:
        return STATE_CLASS_ORDINARY
    default:
        return STATE_CLASS_FIXED
    }
}

func state_mode_code(mode: u8) u8 {
    if mode == serial_protocol.LIFECYCLE_RESET {
        return STATE_MODE_RESET
    }
    if mode == serial_protocol.LIFECYCLE_RELOAD {
        return STATE_MODE_RELOAD
    }
    return STATE_MODE_NONE
}

func state_participation(target: u8) u8 {
    if identity_taxonomy.identity_target_is_lane(target) {
        return STATE_PARTICIPATION_LANE
    }
    class: u8 = state_class(target)
    if class == STATE_CLASS_RETAINED || class == STATE_CLASS_DURABLE {
        return STATE_PARTICIPATION_SERVICE
    }
    return STATE_PARTICIPATION_NONE
}

func state_policy_code(policy: u8) u8 {
    if policy == serial_protocol.POLICY_CLEAR {
        return STATE_POLICY_CLEAR
    }
    if policy == serial_protocol.POLICY_KEEP {
        return STATE_POLICY_KEEP
    }
    return STATE_POLICY_NONE
}

func state_generation_marker(generation: u32) u8 {
    return u8(generation)
}

func state_durability(target: u8) u8 {
    class: u8 = state_class(target)
    if class == STATE_CLASS_DURABLE {
        return STATE_DURABILITY_DURABLE
    }
    if class == STATE_CLASS_RETAINED || class == STATE_CLASS_LANE {
        return STATE_DURABILITY_RETAINED
    }
    return STATE_DURABILITY_NONE
}

func state_durable_level(target: u8) u8 {
    return state_durability(target)
}

func state_durable_requirements(target: u8) u8 {
    if state_durability(target) != STATE_DURABILITY_NONE {
        return STATE_DURABLE_REQ_PERSISTENCE + STATE_DURABLE_REQ_NAMING + STATE_DURABLE_REQ_RECOVERY
    }
    return STATE_DURABLE_REQ_NONE
}

func state_durability_payload(target: u8) [4]u8 {
    payload: [4]u8
    payload[0] = target
    payload[1] = state_durability(target)
    payload[2] = state_durable_level(target)
    payload[3] = state_durable_requirements(target)
    return payload
}

func state_payload(target: u8, class: u8, mode: u8, participation: u8, policy: u8, metadata: u8, generation: u8) [4]u8 {
    payload: [4]u8
    payload[0] = target
    payload[1] = u8((class << 4) + mode)
    payload[2] = u8((participation << 6) + (policy << 4) + metadata)
    payload[3] = generation
    return payload
}

func state_target(payload: [4]u8) u8 {
    return payload[0]
}

func state_class_from_payload(payload: [4]u8) u8 {
    return payload[1] >> 4
}

func state_mode_from_payload(payload: [4]u8) u8 {
    class: u8 = state_class_from_payload(payload)
    return payload[1] - u8(class << 4)
}

func state_participation_from_payload(payload: [4]u8) u8 {
    return payload[2] >> 6
}

func state_policy_from_payload(payload: [4]u8) u8 {
    participation: u8 = state_participation_from_payload(payload)
    tail: u8 = payload[2] - u8(participation << 6)
    return tail >> 4
}

func state_metadata_from_payload(payload: [4]u8) u8 {
    participation: u8 = state_participation_from_payload(payload)
    tail: u8 = payload[2] - u8(participation << 6)
    policy: u8 = tail >> 4
    return tail - u8(policy << 4)
}

func state_generation_from_payload(payload: [4]u8) u8 {
    return payload[3]
}