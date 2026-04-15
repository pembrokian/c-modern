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
//
// The CMD_* constants are shared between the encode side (this module) and
// the decode side (shell_service.mc).  Import this module on both sides to
// prevent client/server protocol drift.

import ipc

const CMD_L: u8 = 76   // 'L'
const CMD_A: u8 = 65   // 'A'
const CMD_T: u8 = 84   // 'T'
const CMD_K: u8 = 75   // 'K'
const CMD_E: u8 = 69   // 'E'
const CMD_S: u8 = 83   // 'S'
const CMD_G: u8 = 71   // 'G'
const CMD_C: u8 = 67   // 'C' — count
const CMD_BANG: u8 = 33  // '!' — end-of-argument sentinel

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
