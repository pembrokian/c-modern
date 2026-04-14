// IPC primitive utilities for the reset-lane kernel.
//
// This module owns the lowest-level IPC helpers used by both the kernel
// dispatcher and callers building observations.  It does not own protocol
// routing, endpoint policy, or service-level dispatch.

func zero_payload() [4]u8 {
    payload: [4]u8
    payload[0] = 0
    payload[1] = 0
    payload[2] = 0
    payload[3] = 0
    return payload
}

func payload_byte(b0: u8, b1: u8, b2: u8, b3: u8) [4]u8 {
    payload: [4]u8
    payload[0] = b0
    payload[1] = b1
    payload[2] = b2
    payload[3] = b3
    return payload
}
