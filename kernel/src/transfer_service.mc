import primitives
import service_effect
import syscall

struct TransferServiceState {
    pid: u32
    slot: u32
}

struct TransferResult {
    state: TransferServiceState
    effect: service_effect.Effect
}

func transfer_init(pid: u32, slot: u32) TransferServiceState {
    return TransferServiceState{ pid: pid, slot: slot }
}

func handle(s: TransferServiceState, m: service_effect.Message) TransferResult {
    if m.received_handle_slot == 0 {
        return TransferResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.InvalidCapability, 0, primitives.zero_payload()) }
    }
    if m.received_handle_count != 1 {
        return TransferResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.InvalidCapability, 0, primitives.zero_payload()) }
    }
    if m.received_endpoint_id == 0 {
        return TransferResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.InvalidCapability, 0, primitives.zero_payload()) }
    }
    return TransferResult{ state: s, effect: service_effect.effect_send(m.source_pid, m.received_endpoint_id, m.payload_len, m.payload) }
}