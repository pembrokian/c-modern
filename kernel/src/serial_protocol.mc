// Serial shell protocol — pure byte encoding.
//
// This layer owns the four-byte ASCII shell wire format only.
// It knows nothing about kernel structures, endpoints, or transport.
// Consumers wrap these payloads into syscall.ReceiveObservation themselves.
//
//   L A <value> !   — log append
//   L T ! !         — log tail
//   K S <key> <val> — kv set
//   K G <key> !     — kv get
//   K C ! !         — kv count (returns count as reply_payload_len)
//   X Q <target> !  — lifecycle query
//   X R <target> !  — explicit restart
//
// The CMD_* constants are shared between the encode side (this module) and
// the decode side (shell_service.mc).  Import this module on both sides to
// prevent client/server protocol drift.

import ipc

const CMD_L: u8 = 76   // 'L'
const CMD_A: u8 = 65   // 'A'
const CMD_T: u8 = 84   // 'T'
const CMD_K: u8 = 75   // 'K'
const CMD_Q: u8 = 81   // 'Q'
const CMD_R: u8 = 82   // 'R' — restart
const CMD_E: u8 = 69   // 'E'
const CMD_S: u8 = 83   // 'S'
const CMD_G: u8 = 71   // 'G'
const CMD_C: u8 = 67   // 'C' — count
const CMD_D: u8 = 68   // 'D' — dequeue
const CMD_I: u8 = 73   // 'I' — issue
const CMD_U: u8 = 85   // 'U' — use
const CMD_X: u8 = 88   // 'X' — lifecycle control family
const CMD_BANG: u8 = 33  // '!' — end-of-argument sentinel

const TARGET_SERIAL: u8 = 83    // 'S'
const TARGET_SHELL: u8 = 72     // 'H'
const TARGET_LOG: u8 = 76       // 'L'
const TARGET_KV: u8 = 75        // 'K'
const TARGET_QUEUE: u8 = 81     // 'Q'
const TARGET_ECHO: u8 = 69      // 'E'
const TARGET_TRANSFER: u8 = 80  // 'P'
const TARGET_TICKET: u8 = 84    // 'T'

const LIFECYCLE_NONE: u8 = 78    // 'N'
const LIFECYCLE_RESET: u8 = 82   // 'R'
const LIFECYCLE_RELOAD: u8 = 76  // 'L'

func encode_echo(left: u8, right: u8) [4]u8 {
    return ipc.payload_byte(CMD_E, CMD_C, left, right)
}

func encode_log_append(value: u8) [4]u8 {
    return ipc.payload_byte(CMD_L, CMD_A, value, CMD_BANG)
}

func encode_log_tail() [4]u8 {
    return ipc.payload_byte(CMD_L, CMD_T, CMD_BANG, CMD_BANG)
}

func encode_kv_set(key: u8, value: u8) [4]u8 {
    return ipc.payload_byte(CMD_K, CMD_S, key, value)
}

func encode_kv_get(key: u8) [4]u8 {
    return ipc.payload_byte(CMD_K, CMD_G, key, CMD_BANG)
}

func encode_kv_count() [4]u8 {
    return ipc.payload_byte(CMD_K, CMD_C, CMD_BANG, CMD_BANG)
}

func encode_lifecycle_query(target: u8) [4]u8 {
    return ipc.payload_byte(CMD_X, CMD_Q, target, CMD_BANG)
}

func encode_lifecycle_restart(target: u8) [4]u8 {
    return ipc.payload_byte(CMD_X, CMD_R, target, CMD_BANG)
}

func encode_queue_enqueue(value: u8) [4]u8 {
    return ipc.payload_byte(CMD_Q, CMD_A, value, CMD_BANG)
}

func encode_queue_dequeue() [4]u8 {
    return ipc.payload_byte(CMD_Q, CMD_D, CMD_BANG, CMD_BANG)
}

func encode_ticket_issue() [4]u8 {
    return ipc.payload_byte(CMD_T, CMD_I, CMD_BANG, CMD_BANG)
}

func encode_ticket_use(epoch: u8, id: u8) [4]u8 {
    return ipc.payload_byte(CMD_T, CMD_U, epoch, id)
}
