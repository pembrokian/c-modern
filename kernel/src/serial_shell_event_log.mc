import serial_service
import serial_shell_path
import syscall

const EVENT_LOG_CAPACITY: usize = 4
const SERIAL_INVALID_BYTE: u8 = 255
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

struct TracedSerialShellStep {
    result: serial_shell_path.SerialShellPathResult
    event_log: SerialShellEventLog
}

func event_log_init() SerialShellEventLog {
    return SerialShellEventLog{ slot0: 0, slot1: 0, slot2: 0, slot3: 0, start: 0, len: 0 }
}

func normalize_slot_index(raw_index: usize) usize {
    if raw_index < EVENT_LOG_CAPACITY {
        return raw_index
    }
    return raw_index - EVENT_LOG_CAPACITY
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

func ingress_event_code(observation: syscall.ReceiveObservation) u32 {
    if observation.payload_len == 0 {
        return 0
    }
    if observation.payload[0] == SERIAL_INVALID_BYTE {
        return EVENT_SERIAL_REJECTED
    }
    if serial_service.debug_ingress(observation) == 0 {
        return 0
    }
    return EVENT_SERIAL_BUFFERED
}

func reply_event_code(result: serial_shell_path.SerialShellPathResult) u32 {
    if result.reply_status == syscall.SyscallStatus.Ok {
        return EVENT_SHELL_REPLY_OK
    }
    if result.reply_status == syscall.SyscallStatus.InvalidArgument {
        return EVENT_SHELL_REPLY_INVALID
    }
    return 0
}

func trace_step(state: serial_shell_path.SerialShellPathState, event_log: SerialShellEventLog, observation: syscall.ReceiveObservation) TracedSerialShellStep {
    result: serial_shell_path.SerialShellPathResult = serial_shell_path.serial_to_shell_step(state, observation)
    next_log: SerialShellEventLog = event_log
    event_code: u32 = ingress_event_code(observation)

    if event_code != 0 {
        next_log = event_log_push(next_log, event_code)
    }
    if serial_shell_path.debug_step(result) != 0 {
        next_log = event_log_push(next_log, EVENT_SHELL_FORWARDED)
        event_code = reply_event_code(result)
        if event_code != 0 {
            next_log = event_log_push(next_log, event_code)
        }
        if result.state.serial_state.len == 0 {
            next_log = event_log_push(next_log, EVENT_SERIAL_CLEARED)
        }
    }

    return TracedSerialShellStep{ result: result, event_log: next_log }
}