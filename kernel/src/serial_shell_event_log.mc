import event_codes
import service_effect

// Re-export event code constants so callers that already import this module
// for event_log_push/event_log_entry do not need a separate import.
const EVENT_SERIAL_BUFFERED: u32 = event_codes.EVENT_SERIAL_BUFFERED
const EVENT_SERIAL_REJECTED: u32 = event_codes.EVENT_SERIAL_REJECTED
const EVENT_SHELL_FORWARDED: u32 = event_codes.EVENT_SHELL_FORWARDED
const EVENT_SHELL_REPLY_OK: u32 = event_codes.EVENT_SHELL_REPLY_OK
const EVENT_SHELL_REPLY_INVALID: u32 = event_codes.EVENT_SHELL_REPLY_INVALID
const EVENT_SERIAL_CLEARED: u32 = event_codes.EVENT_SERIAL_CLEARED

const EVENT_LOG_CAPACITY: usize = 4

struct SerialShellEventLog {
    slots: [4]u32
    start: usize
    len: usize
}

func event_log_init() SerialShellEventLog {
    slots: [4]u32
    slots[0] = 0
    slots[1] = 0
    slots[2] = 0
    slots[3] = 0
    return SerialShellEventLog{ slots: slots, start: 0, len: 0 }
}

func normalize_slot_index(raw_index: usize) usize {
    next_index: usize = raw_index
    while next_index >= EVENT_LOG_CAPACITY {
        next_index = next_index - EVENT_LOG_CAPACITY
    }
    return next_index
}

func with_slot(log: SerialShellEventLog, slot_index: usize, event_code: u32) SerialShellEventLog {
    next_slots: [4]u32 = log.slots
    next_slots[slot_index] = event_code
    return SerialShellEventLog{ slots: next_slots, start: log.start, len: log.len }
}

func event_log_push(log: SerialShellEventLog, event_code: u32) SerialShellEventLog {
    insert_index: usize = log.start + log.len
    next_log: SerialShellEventLog = log
    next_start: usize = log.start
    next_len: usize = log.len

    if next_len == EVENT_LOG_CAPACITY {
        insert_index = log.start
        next_start = normalize_slot_index(log.start + 1)
    } else {
        next_len = next_len + 1
    }

    next_log = with_slot(next_log, normalize_slot_index(insert_index), event_code)
    return SerialShellEventLog{ slots: next_log.slots, start: next_start, len: next_len }
}

func event_log_len(log: SerialShellEventLog) usize {
    return log.len
}

func event_log_entry(log: SerialShellEventLog, logical_index: usize) u32 {
    if logical_index >= log.len {
        return 0
    }
    return log.slots[normalize_slot_index(log.start + logical_index)]
}

func append_effect_events(log: SerialShellEventLog, effect: service_effect.Effect) SerialShellEventLog {
    next_log: SerialShellEventLog = log
    for i in 0..effect.event_count {
        next_log = event_log_push(next_log, service_effect.effect_event(effect, i))
    }
    return next_log
}
