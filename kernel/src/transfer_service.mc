import primitives
import service_effect
import service_topology
import syscall

const TRANSFER_CMD_BIND: u8 = 66
const TRANSFER_CMD_LOG: u8 = 76
const TRANSFER_CMD_KV: u8 = 75

struct TransferServiceState {
    pid: u32
    slot: u32
    left: u32
    right: u32
}

struct TransferResult {
    state: TransferServiceState
    effect: service_effect.Effect
}

func transfer_init(pid: u32, slot: u32) TransferServiceState {
    return TransferServiceState{ pid: pid, slot: slot, left: 0, right: 0 }
}

func transferwith(s: TransferServiceState, left: u32, right: u32) TransferServiceState {
    return TransferServiceState{ pid: s.pid, slot: s.slot, left: left, right: right }
}

func transfer_ok(s: TransferServiceState) TransferResult {
    return TransferResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.Ok, 0, primitives.zero_payload()) }
}

func transfer_invalid(s: TransferServiceState) TransferResult {
    return TransferResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.InvalidCapability, 0, primitives.zero_payload()) }
}

func transfer_badarg(s: TransferServiceState) TransferResult {
    return TransferResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.InvalidArgument, 0, primitives.zero_payload()) }
}

func transfer_pair_ok(left: u32, right: u32) bool {
    if left == 0 {
        return false
    }
    if right == 0 {
        return false
    }
    if left == right {
        return false
    }
    if left != service_topology.LOG_ENDPOINT_ID && left != service_topology.KV_ENDPOINT_ID {
        return false
    }
    if right != service_topology.LOG_ENDPOINT_ID && right != service_topology.KV_ENDPOINT_ID {
        return false
    }
    if left == service_topology.LOG_ENDPOINT_ID && right != service_topology.KV_ENDPOINT_ID {
        return false
    }
    if left == service_topology.KV_ENDPOINT_ID && right != service_topology.LOG_ENDPOINT_ID {
        return false
    }
    return true
}

func transfer_bind(s: TransferServiceState, m: service_effect.Message) TransferResult {
    if m.received_handle_count != 2 {
        return transfer_invalid(s)
    }
    if !transfer_pair_ok(m.received_endpoint_id, m.received_endpoint_id_1) {
        return transfer_invalid(s)
    }
    return transfer_ok(transferwith(s, m.received_endpoint_id, m.received_endpoint_id_1))
}

func onebyte(value: u8) [4]u8 {
    payload: [4]u8 = primitives.zero_payload()
    payload[0] = value
    return payload
}

func transfer_direct(s: TransferServiceState, m: service_effect.Message) TransferResult {
    if m.received_handle_count != 1 {
        return transfer_invalid(s)
    }
    if m.received_endpoint_id == 0 {
        return transfer_invalid(s)
    }
    return TransferResult{ state: s, effect: service_effect.effect_send(m.source_pid, m.received_endpoint_id, m.payload_len, m.payload) }
}

func transfer_log(s: TransferServiceState, m: service_effect.Message) TransferResult {
    if s.left != service_topology.LOG_ENDPOINT_ID && s.right != service_topology.LOG_ENDPOINT_ID {
        return transfer_invalid(s)
    }
    if m.payload_len < 2 {
        return transfer_badarg(s)
    }
    return TransferResult{ state: s, effect: service_effect.effect_send(m.source_pid, service_topology.LOG_ENDPOINT_ID, 1, onebyte(m.payload[1])) }
}

func transfer_kv(s: TransferServiceState, m: service_effect.Message) TransferResult {
    if s.left != service_topology.KV_ENDPOINT_ID && s.right != service_topology.KV_ENDPOINT_ID {
        return transfer_invalid(s)
    }
    if m.payload_len < 3 {
        return transfer_badarg(s)
    }
    payload: [4]u8 = primitives.zero_payload()
    payload[0] = m.payload[1]
    payload[1] = m.payload[2]
    return TransferResult{ state: s, effect: service_effect.effect_send(m.source_pid, service_topology.KV_ENDPOINT_ID, 2, payload) }
}

func handle(s: TransferServiceState, m: service_effect.Message) TransferResult {
    if m.received_handle_count == 1 {
        return transfer_direct(s, m)
    }
    if m.received_handle_count != 0 {
        if m.payload_len == 0 {
            return transfer_badarg(s)
        }
        if m.payload[0] != TRANSFER_CMD_BIND {
            return transfer_badarg(s)
        }
        return transfer_bind(s, m)
    }
    if m.payload_len == 0 {
        return transfer_badarg(s)
    }
    if m.payload[0] == TRANSFER_CMD_LOG {
        return transfer_log(s, m)
    }
    if m.payload[0] == TRANSFER_CMD_KV {
        return transfer_kv(s, m)
    }
    return transfer_badarg(s)
}