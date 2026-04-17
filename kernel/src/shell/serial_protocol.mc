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
//   Q A <value> !   — queue enqueue
//   Q D ! !         — queue dequeue
//   Q C ! !         — queue count (returns count as reply_payload_len)
//   Q P ! !         — queue peek
//   X A <target> !  — authority inspection query
//   X C <target> !  — compact service-state query
//   X D <target> !  — retained-vs-durable boundary query
//   X Q <target> !  — lifecycle query
//   X P <target> !  — lifecycle policy query
//   X S <target> !  — retained summary query
//   X M <target> !  — retained/restart comparison query
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
const CMD_P: u8 = 80   // 'P' — peek
const CMD_U: u8 = 85   // 'U' — use
const CMD_X: u8 = 88   // 'X' — lifecycle control family
const CMD_BANG: u8 = 33  // '!' — end-of-argument sentinel
const CMD_W: u8 = 87   // 'W' — stall-count query; also write sub-op within CMD_F
const CMD_F: u8 = 70   // 'F' — file command family
const CMD_M: u8 = 77   // 'M' — timer command family
const CMD_N: u8 = 78   // 'N' — object store command family
const CMD_J: u8 = 74   // 'J' — task command family
const CMD_Y: u8 = 89   // 'Y' — journal command family
const CMD_O: u8 = 79   // 'O' — workflow command family
const CMD_V: u8 = 86   // 'V' — completion mailbox command family
const CMD_Z: u8 = 90   // 'Z' — lease command family

const TARGET_SERIAL: u8 = 83    // 'S'
const TARGET_SHELL: u8 = 72     // 'H'
const TARGET_AUDIT: u8 = 65     // 'A'
const TARGET_LOG: u8 = 76       // 'L'
const TARGET_KV: u8 = 75        // 'K'
const TARGET_QUEUE: u8 = 81     // 'Q'
const TARGET_WORKSET: u8 = 87   // 'W'
const TARGET_ECHO: u8 = 69      // 'E'
const TARGET_TRANSFER: u8 = 80  // 'P'
const TARGET_TICKET: u8 = 84    // 'T'
const TARGET_FILE: u8 = 70      // 'F'
const TARGET_TIMER: u8 = 77     // 'M'
const TARGET_TASK: u8 = 74      // 'J'
const TARGET_JOURNAL: u8 = 85   // 'U'
const TARGET_WORKFLOW: u8 = 79  // 'O'
const TARGET_OBJECT_STORE: u8 = 78  // 'N'
const TARGET_LEASE: u8 = 90     // 'Z'
const TARGET_COMPLETION: u8 = 66  // 'B'

const PARTICIPANT_NONE: u8 = 78  // 'N'
const POLICY_CLEAR: u8 = 67      // 'C'
const POLICY_KEEP: u8 = 75       // 'K'

const LIFECYCLE_NONE: u8 = 78    // 'N'
const LIFECYCLE_RESET: u8 = 82   // 'R'
const LIFECYCLE_RELOAD: u8 = 76  // 'L'

const AUTHORITY_PUBLIC: u8 = 80       // 'P'
const AUTHORITY_TRANSFER: u8 = 84     // 'T'
const AUTHORITY_RETAINED: u8 = 82     // 'R'
const AUTHORITY_COORDINATED: u8 = 67  // 'C'
const AUTHORITY_SHELL: u8 = 83        // 'S'
const AUTHORITY_DURABLE: u8 = 68      // 'D'

const AUTHORITY_TRANSFER_NO: u8 = 78   // 'N'
const AUTHORITY_TRANSFER_YES: u8 = 84  // 'T'

const AUTHORITY_SCOPE_NONE: u8 = 78         // 'N'
const AUTHORITY_SCOPE_RETAINED: u8 = 82     // 'R'
const AUTHORITY_SCOPE_COORDINATED: u8 = 67  // 'C'
const AUTHORITY_SCOPE_DURABLE: u8 = 68      // 'D'

func encode_no_args(cmd0: u8, cmd1: u8) [4]u8 {
    return ipc.payload_byte(cmd0, cmd1, CMD_BANG, CMD_BANG)
}

func encode_one_arg(cmd0: u8, cmd1: u8, arg0: u8) [4]u8 {
    return ipc.payload_byte(cmd0, cmd1, arg0, CMD_BANG)
}

func encode_two_args(cmd0: u8, cmd1: u8, arg0: u8, arg1: u8) [4]u8 {
    return ipc.payload_byte(cmd0, cmd1, arg0, arg1)
}

func encode_echo(left: u8, right: u8) [4]u8 {
    return encode_two_args(CMD_E, CMD_C, left, right)
}

func encode_log_append(value: u8) [4]u8 {
    return encode_one_arg(CMD_L, CMD_A, value)
}

func encode_log_tail() [4]u8 {
    return encode_no_args(CMD_L, CMD_T)
}

func encode_kv_set(key: u8, value: u8) [4]u8 {
    return encode_two_args(CMD_K, CMD_S, key, value)
}

func encode_kv_get(key: u8) [4]u8 {
    return encode_one_arg(CMD_K, CMD_G, key)
}

func encode_kv_count() [4]u8 {
    return encode_no_args(CMD_K, CMD_C)
}

func encode_lifecycle_query(target: u8) [4]u8 {
    return encode_one_arg(CMD_X, CMD_Q, target)
}

func encode_lifecycle_authority(target: u8) [4]u8 {
    return encode_one_arg(CMD_X, CMD_A, target)
}

func encode_lifecycle_state(target: u8) [4]u8 {
    return encode_one_arg(CMD_X, CMD_C, target)
}

func encode_lifecycle_durability(target: u8) [4]u8 {
    return encode_one_arg(CMD_X, CMD_D, target)
}

func encode_lifecycle_identity(target: u8) [4]u8 {
    return encode_one_arg(CMD_X, CMD_I, target)
}

func encode_lifecycle_policy(target: u8) [4]u8 {
    return encode_one_arg(CMD_X, CMD_P, target)
}

func encode_lifecycle_summary(target: u8) [4]u8 {
    return encode_one_arg(CMD_X, CMD_S, target)
}

func encode_lifecycle_compare(target: u8) [4]u8 {
    return encode_one_arg(CMD_X, CMD_M, target)
}

func encode_lifecycle_restart(target: u8) [4]u8 {
    return encode_one_arg(CMD_X, CMD_R, target)
}

func encode_queue_enqueue(value: u8) [4]u8 {
    return encode_one_arg(CMD_Q, CMD_A, value)
}

func encode_queue_dequeue() [4]u8 {
    return encode_no_args(CMD_Q, CMD_D)
}

func encode_queue_count() [4]u8 {
    return encode_no_args(CMD_Q, CMD_C)
}

func encode_queue_peek() [4]u8 {
    return encode_no_args(CMD_Q, CMD_P)
}

func encode_queue_stall_count() [4]u8 {
    return encode_no_args(CMD_Q, CMD_W)
}

func encode_ticket_issue() [4]u8 {
    return encode_no_args(CMD_T, CMD_I)
}

func encode_ticket_use(epoch: u8, id: u8) [4]u8 {
    return encode_two_args(CMD_T, CMD_U, epoch, id)
}

func encode_file_create(name: u8) [4]u8 {
    return encode_one_arg(CMD_F, CMD_C, name)
}

func encode_file_write(name: u8, val: u8) [4]u8 {
    return encode_two_args(CMD_F, CMD_W, name, val)
}

func encode_file_read(name: u8) [4]u8 {
    return encode_one_arg(CMD_F, CMD_R, name)
}

func encode_file_count() [4]u8 {
    return encode_no_args(CMD_F, CMD_L)
}

func encode_object_create(name: u8, value: u8) [4]u8 {
    return encode_two_args(CMD_N, CMD_C, name, value)
}

func encode_object_read(name: u8) [4]u8 {
    return encode_one_arg(CMD_N, CMD_R, name)
}

func encode_object_replace(name: u8, value: u8) [4]u8 {
    return encode_two_args(CMD_N, CMD_W, name, value)
}

func encode_timer_create(id: u8, due: u8) [4]u8 {
    return encode_two_args(CMD_M, CMD_C, id, due)
}

func encode_timer_cancel(id: u8) [4]u8 {
    return encode_one_arg(CMD_M, CMD_X, id)
}

func encode_timer_query(id: u8) [4]u8 {
    return encode_one_arg(CMD_M, CMD_Q, id)
}

func encode_timer_expired(window: u8) [4]u8 {
    return encode_one_arg(CMD_M, CMD_E, window)
}

func encode_task_submit(opcode: u8) [4]u8 {
    return encode_one_arg(CMD_J, CMD_S, opcode)
}

func encode_task_query(id: u8) [4]u8 {
    return encode_one_arg(CMD_J, CMD_Q, id)
}

func encode_task_cancel(id: u8) [4]u8 {
    return encode_one_arg(CMD_J, CMD_C, id)
}

func encode_task_complete(id: u8) [4]u8 {
    return encode_one_arg(CMD_J, CMD_D, id)
}

func encode_task_list(window: u8) [4]u8 {
    return encode_one_arg(CMD_J, CMD_L, window)
}

func encode_journal_append(name: u8, value: u8) [4]u8 {
    return encode_two_args(CMD_Y, CMD_A, name, value)
}

func encode_journal_replay(name: u8) [4]u8 {
    return encode_one_arg(CMD_Y, CMD_R, name)
}

func encode_journal_clear(name: u8) [4]u8 {
    return encode_one_arg(CMD_Y, CMD_C, name)
}

func encode_workflow_schedule(id: u8, due: u8) [4]u8 {
    return encode_two_args(CMD_O, CMD_S, id, due)
}

func encode_workflow_update(name: u8, value: u8) [4]u8 {
    return encode_two_args(CMD_O, CMD_W, name, value)
}

func encode_workflow_query(id: u8) [4]u8 {
    return encode_one_arg(CMD_O, CMD_Q, id)
}

func encode_workflow_cancel(id: u8) [4]u8 {
    return encode_one_arg(CMD_O, CMD_C, id)
}

func encode_lease_issue(workflow: u8) [4]u8 {
    return encode_one_arg(CMD_Z, CMD_I, workflow)
}

func encode_lease_consume(id: u8, workflow: u8) [4]u8 {
    return encode_two_args(CMD_Z, CMD_U, id, workflow)
}

func encode_lease_issue_object_update(name: u8, value: u8) [4]u8 {
    return encode_two_args(CMD_Z, CMD_W, name, value)
}

func encode_lease_consume_object_update(id: u8) [4]u8 {
    return encode_one_arg(CMD_Z, CMD_D, id)
}

func encode_completion_fetch() [4]u8 {
    return encode_no_args(CMD_V, CMD_F)
}

func encode_completion_ack() [4]u8 {
    return encode_no_args(CMD_V, CMD_A)
}

func encode_completion_count() [4]u8 {
    return encode_no_args(CMD_V, CMD_C)
}
