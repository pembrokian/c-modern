import ipc
import kv_service
import service_effect
import syscall

func smoke_set_and_get_returns_value() bool {
    state: kv_service.KvServiceState = kv_service.kv_init(21, 1)
    payload: [4]u8 = ipc.zero_payload()
    kv_result: kv_service.KvResult
    effect: service_effect.Effect

    payload[0] = 1
    payload[1] = 42
    kv_result = kv_service.handle(state, service_effect.message(7, 90, 2, payload))
    state = kv_result.state
    effect = kv_result.effect
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    payload[1] = 0
    kv_result = kv_service.handle(state, service_effect.message(7, 90, 1, payload))
    effect = kv_result.effect
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload_len(effect) != 2 {
        return false
    }
    if service_effect.effect_reply_payload(effect)[1] != 42 {
        return false
    }
    return true
}

func smoke_get_missing_key_fails() bool {
    state: kv_service.KvServiceState = kv_service.kv_init(21, 1)
    payload: [4]u8 = ipc.zero_payload()
    kv_result: kv_service.KvResult
    effect: service_effect.Effect

    payload[0] = 99
    kv_result = kv_service.handle(state, service_effect.message(7, 90, 1, payload))
    effect = kv_result.effect
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.InvalidArgument {
        return false
    }
    return true
}

func smoke_overwrite_updates_value() bool {
    state: kv_service.KvServiceState = kv_service.kv_init(21, 1)
    payload: [4]u8 = ipc.zero_payload()
    kv_result: kv_service.KvResult
    effect: service_effect.Effect

    payload[0] = 3
    payload[1] = 10
    kv_result = kv_service.handle(state, service_effect.message(7, 90, 2, payload))
    state = kv_result.state
    effect = kv_result.effect
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    payload[1] = 99
    kv_result = kv_service.handle(state, service_effect.message(7, 90, 2, payload))
    state = kv_result.state
    effect = kv_result.effect
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    kv_result = kv_service.handle(state, service_effect.message(7, 90, 1, payload))
    effect = kv_result.effect
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }
    if service_effect.effect_reply_payload(effect)[1] != 99 {
        return false
    }
    return true
}

func smoke_distinct_keys_are_independent() bool {
    state: kv_service.KvServiceState = kv_service.kv_init(21, 1)
    payload: [4]u8 = ipc.zero_payload()
    kv_result: kv_service.KvResult
    effect: service_effect.Effect

    payload[0] = 1
    payload[1] = 10
    kv_result = kv_service.handle(state, service_effect.message(7, 90, 2, payload))
    state = kv_result.state
    effect = kv_result.effect
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    payload[0] = 2
    payload[1] = 20
    kv_result = kv_service.handle(state, service_effect.message(7, 90, 2, payload))
    state = kv_result.state
    effect = kv_result.effect
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.Ok {
        return false
    }

    payload[0] = 1
    kv_result = kv_service.handle(state, service_effect.message(7, 90, 1, payload))
    effect = kv_result.effect
    if service_effect.effect_reply_payload(effect)[1] != 10 {
        return false
    }

    payload[0] = 2
    kv_result = kv_service.handle(state, service_effect.message(7, 90, 1, payload))
    effect = kv_result.effect
    if service_effect.effect_reply_payload(effect)[1] != 20 {
        return false
    }
    return true
}

func main() i32 {
    if !smoke_set_and_get_returns_value() {
        return 1
    }
    if !smoke_get_missing_key_fails() {
        return 1
    }
    if !smoke_overwrite_updates_value() {
        return 1
    }
    if !smoke_distinct_keys_are_independent() {
        return 1
    }
    return 0
}
