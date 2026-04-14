import service_effect

const EVENT_LOG_CAPACITY: usize = 4
const EVENT_SERIAL_BUFFERED: u32 = 1
const EVENT_SERIAL_REJECTED: u32 = 2
const EVENT_SHELL_FORWARDED: u32 = 3
const EVENT_SHELL_REPLY_OK: u32 = 4
const EVENT_SHELL_REPLY_INVALID: u32 = 5
const EVENT_SERIAL_CLEARED: u32 = 6

struct SerialShellEventLog {
    slot0: u32
    slot1: u32
    slot2: u32
    slot3: u32
    start: usize
    len: usize
}

func event_log_init() SerialShellEventLog {
    return SerialShellEventLog{ slot0: 0, slot1: 0, slot2: 0, slot3: 0, start: 0, len: 0 }
}

func normalize_slot_index(raw_index: usize) usize {
    next_index: usize = raw_index
    while next_index >= EVENT_LOG_CAPACITY {
        next_index = next_index - EVENT_LOG_CAPACITY
    }
    return next_index
}

func slot_value(log: SerialShellEventLog, slot_index: usize) u32 {
    if slot_index == 0 {
        return log.slot0
    }
    if slot_index == 1 {
        return log.slot1
    }
    if slot_index == 2 {
        return log.slot2
    }
    return log.slot3
}

func with_slot(log: SerialShellEventLog, slot_index: usize, event_code: u32) SerialShellEventLog {
    if slot_index == 0 {
        return SerialShellEventLog{ slot0: event_code, slot1: log.slot1, slot2: log.slot2, slot3: log.slot3, start: log.start, len: log.len }
    }
    if slot_index == 1 {
        return SerialShellEventLog{ slot0: log.slot0, slot1: event_code, slot2: log.slot2, slot3: log.slot3, start: log.start, len: log.len }
    }
    if slot_index == 2 {
        return SerialShellEventLog{ slot0: log.slot0, slot1: log.slot1, slot2: event_code, slot3: log.slot3, start: log.start, len: log.len }
    }
    return SerialShellEventLog{ slot0: log.slot0, slot1: log.slot1, slot2: log.slot2, slot3: event_code, start: log.start, len: log.len }
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
    return SerialShellEventLog{ slot0: next_log.slot0, slot1: next_log.slot1, slot2: next_log.slot2, slot3: next_log.slot3, start: next_start, len: next_len }
}

func event_log_len(log: SerialShellEventLog) usize {
    return log.len
}

func event_log_entry(log: SerialShellEventLog, logical_index: usize) u32 {
    if logical_index >= log.len {
        return 0
    }
    return slot_value(log, normalize_slot_index(log.start + logical_index))
}

func append_effect_events(log: SerialShellEventLog, effect: service_effect.Effect) SerialShellEventLog {
    next_log: SerialShellEventLog = log
    index: usize = 0
    while index < service_effect.effect_event_count(effect) {
        next_log = event_log_push(next_log, service_effect.effect_event(effect, index))
        index = index + 1
    }
    return next_log
}

func event_serial_buffered() u32 {
    return EVENT_SERIAL_BUFFERED
}

func event_serial_rejected() u32 {
    return EVENT_SERIAL_REJECTED
}

func event_shell_forwarded() u32 {
    return EVENT_SHELL_FORWARDED
}

func event_shell_reply_ok() u32 {
    return EVENT_SHELL_REPLY_OK
}

func event_shell_reply_invalid() u32 {
    return EVENT_SHELL_REPLY_INVALID
}

func event_serial_cleared() u32 {
    return EVENT_SERIAL_CLEARED
}
