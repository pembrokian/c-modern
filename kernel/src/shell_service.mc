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
    owner_pid: u32
    endpoint_handle_slot: u32
    log_endpoint_id: u32
    kv_endpoint_id: u32
}

func shell_init(owner_pid: u32, endpoint_handle_slot: u32, log_endpoint_id: u32, kv_endpoint_id: u32) ShellServiceState {
    return ShellServiceState{ owner_pid: owner_pid, endpoint_handle_slot: endpoint_handle_slot, log_endpoint_id: log_endpoint_id, kv_endpoint_id: kv_endpoint_id }
}

func service_state(owner_pid: u32, endpoint_handle_slot: u32, log_endpoint_id: u32, kv_endpoint_id: u32) ShellServiceState {
    return shell_init(owner_pid, endpoint_handle_slot, log_endpoint_id, kv_endpoint_id)
}

func invalid_effect() service_effect.Effect {
    payload: [4]u8 = primitives.zero_payload()
    payload[0] = SHELL_INVALID_REPLY
    return service_effect.effect_reply(syscall.SyscallStatus.InvalidArgument, 1, payload)
}

func handle(state: ShellServiceState, msg: service_effect.Message) service_effect.Effect {
    payload: [4]u8 = primitives.zero_payload()

    if state.owner_pid == 0 || msg.payload_len != 4 {
        return invalid_effect()
    }

    if msg.payload[0] == CMD_E && msg.payload[1] == CMD_C {
        payload[0] = msg.payload[2]
        payload[1] = msg.payload[3]
        return service_effect.effect_reply(syscall.SyscallStatus.Ok, 2, payload)
    }

    if msg.payload[0] == CMD_L && msg.payload[1] == CMD_A && msg.payload[3] == CMD_BANG {
        payload[0] = msg.payload[2]
        return service_effect.effect_send(state.owner_pid, state.log_endpoint_id, 1, payload)
    }

    if msg.payload[0] == CMD_L && msg.payload[1] == CMD_T && msg.payload[2] == CMD_BANG && msg.payload[3] == CMD_BANG {
        return service_effect.effect_send(state.owner_pid, state.log_endpoint_id, 0, payload)
    }

    if msg.payload[0] == CMD_K && msg.payload[1] == CMD_S {
        payload[0] = msg.payload[2]
        payload[1] = msg.payload[3]
        return service_effect.effect_send(state.owner_pid, state.kv_endpoint_id, 2, payload)
    }

    if msg.payload[0] == CMD_K && msg.payload[1] == CMD_G && msg.payload[3] == CMD_BANG {
        payload[0] = msg.payload[2]
        return service_effect.effect_send(state.owner_pid, state.kv_endpoint_id, 1, payload)
    }

    return invalid_effect()
}

func debug_request(state: ShellServiceState, payload_len: usize) u32 {
    if state.owner_pid == 0 || payload_len == 0 {
        return 0
    }
    return 1
}
