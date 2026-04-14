import primitives
import service_effect
import syscall

const SHELL_INVALID_REPLY: u8 = 63  // '?'
const CMD_E: u8 = 69   // 'E'
const CMD_C: u8 = 67   // 'C'
const CMD_L: u8 = 76   // 'L'
const CMD_A: u8 = 65   // 'A'
const CMD_T: u8 = 84   // 'T'
const CMD_K: u8 = 75   // 'K'
const CMD_S: u8 = 83   // 'S'
const CMD_G: u8 = 71   // 'G'
const CMD_BANG: u8 = 33  // '!' — end-of-argument sentinel

struct ShellServiceState {
    pid: u32
    slot: u32
    log_endpoint_id: u32
    kv_endpoint_id: u32
}

func shell_init(pid: u32, slot: u32, log_endpoint_id: u32, kv_endpoint_id: u32) ShellServiceState {
    return ShellServiceState{ pid: pid, slot: slot, log_endpoint_id: log_endpoint_id, kv_endpoint_id: kv_endpoint_id }
}

func invalid_effect() service_effect.Effect {
    payload: [4]u8 = primitives.zero_payload()
    payload[0] = SHELL_INVALID_REPLY
    return service_effect.effect_reply(syscall.SyscallStatus.InvalidArgument, 1, payload)
}

func handle(s: ShellServiceState, m: service_effect.Message) service_effect.Effect {
    payload: [4]u8 = primitives.zero_payload()

    if s.pid == 0 || m.payload_len != 4 {
        return invalid_effect()
    }

    if m.payload[0] == CMD_E && m.payload[1] == CMD_C {
        payload[0] = m.payload[2]
        payload[1] = m.payload[3]
        return service_effect.effect_reply(syscall.SyscallStatus.Ok, 2, payload)
    }

    if m.payload[0] == CMD_L && m.payload[1] == CMD_A && m.payload[3] == CMD_BANG {
        payload[0] = m.payload[2]
        return service_effect.effect_send(s.pid, s.log_endpoint_id, 1, payload)
    }

    if m.payload[0] == CMD_L && m.payload[1] == CMD_T && m.payload[2] == CMD_BANG && m.payload[3] == CMD_BANG {
        return service_effect.effect_send(s.pid, s.log_endpoint_id, 0, payload)
    }

    if m.payload[0] == CMD_K && m.payload[1] == CMD_S {
        payload[0] = m.payload[2]
        payload[1] = m.payload[3]
        return service_effect.effect_send(s.pid, s.kv_endpoint_id, 2, payload)
    }

    if m.payload[0] == CMD_K && m.payload[1] == CMD_G && m.payload[3] == CMD_BANG {
        payload[0] = m.payload[2]
        return service_effect.effect_send(s.pid, s.kv_endpoint_id, 1, payload)
    }

    return invalid_effect()
}

func debug_request(s: ShellServiceState, len: usize) u32 {
    if s.pid == 0 || len == 0 {
        return 0
    }
    return 1
}
