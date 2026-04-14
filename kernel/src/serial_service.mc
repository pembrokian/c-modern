import primitives
import syscall

const SERIAL_BUFFER_CAPACITY: usize = 4
const SERIAL_INVALID_BYTE: u8 = 255

struct SerialServiceState {
    pid: u32
    slot: u32
    buffer: [4]u8
    len: usize
    reply_status: syscall.SyscallStatus
    reply_len: usize
    reply_payload: [4]u8
}

func serial_init(pid: u32, slot: u32) SerialServiceState {
    return SerialServiceState{ pid: pid, slot: slot, buffer: primitives.zero_payload(), len: 0, reply_status: syscall.SyscallStatus.None, reply_len: 0, reply_payload: primitives.zero_payload() }
}

func serialwith(s: SerialServiceState, buf: [4]u8, blen: usize, rs: syscall.SyscallStatus, rlen: usize, rp: [4]u8) SerialServiceState {
    return SerialServiceState{ pid: s.pid, slot: s.slot, buffer: buf, len: blen, reply_status: rs, reply_len: rlen, reply_payload: rp }
}

func serial_clear_reply(s: SerialServiceState) SerialServiceState {
    return serialwith(s, s.buffer, s.len, syscall.SyscallStatus.None, 0, primitives.zero_payload())
}

func serial_reply_status(s: SerialServiceState) syscall.SyscallStatus {
    return s.reply_status
}

func serial_reply_len(s: SerialServiceState) usize {
    return s.reply_len
}

func serial_reply_payload(s: SerialServiceState) [4]u8 {
    return s.reply_payload
}

func serial_has_pending_reply(s: SerialServiceState) u32 {
    if s.reply_status == syscall.SyscallStatus.None {
        return 0
    }
    return 1
}

func serial_forwarded(s: SerialServiceState) u32 {
    return serial_has_pending_reply(s)
}

func serial_on_receive(s: SerialServiceState, obs: syscall.ReceiveObservation) SerialServiceState {
    next: SerialServiceState = serial_clear_reply(s)
    if obs.payload_len == 0 {
        return next
    }
    if obs.payload[0] == SERIAL_INVALID_BYTE {
        return next
    }
    // Bytes beyond SERIAL_BUFFER_CAPACITY are intentionally dropped.
    next_buffer: [4]u8 = next.buffer
    next_len: usize = next.len
    for i in 0..obs.payload_len {
        if next_len < SERIAL_BUFFER_CAPACITY {
            next_buffer[next_len] = obs.payload[i]
            next_len = next_len + 1
        }
    }
    return serialwith(next, next_buffer, next_len, syscall.SyscallStatus.None, 0, primitives.zero_payload())
}

func serial_forward_request_len(s: SerialServiceState) usize {
    return s.len
}

func serial_forward_request_payload(s: SerialServiceState) [4]u8 {
    return s.buffer
}

func serial_clear(s: SerialServiceState) SerialServiceState {
    return serialwith(s, primitives.zero_payload(), 0, s.reply_status, s.reply_len, s.reply_payload)
}

func serial_on_reply(s: SerialServiceState, obs: syscall.ReceiveObservation) SerialServiceState {
    if obs.status == syscall.SyscallStatus.None {
        return s
    }
    cleared: SerialServiceState = serial_clear(s)
    return serialwith(cleared, cleared.buffer, cleared.len, obs.status, obs.payload_len, obs.payload)
}