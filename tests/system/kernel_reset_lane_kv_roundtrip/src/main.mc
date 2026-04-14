import ipc
import kv_service
import service_effect
import syscall

func smoke_set_and_get_returns_value() bool {
    state: kv_service.KvServiceState = kv_service.kv_init(21, 1)
    payload: [4]u8 = ipc.zero_payload()
    effect: service_effect.Effect

    payload[0] = 1
    payload[1] = 42
    effect = kv_service.handle(&state, service_effect.message(7, 90, 2, payload))
    if service_effect.effect_has_send(effect) == 0 {
        return false
    }
    if service_effect.effect_send_endpoint_id(effect) != 12 {
        return false
    }

    payload[1] = 0
    effect = kv_service.handle(&state, service_effect.message(7, 90, 1, payload))
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
    effect: service_effect.Effect

    payload[0] = 99
    effect = kv_service.handle(&state, service_effect.message(7, 90, 1, payload))
    if service_effect.effect_reply_status(effect) != syscall.SyscallStatus.InvalidArgument {
        return false
    }
    return true
}

func smoke_overwrite_updates_value() bool {
    state: kv_service.KvServiceState = kv_service.kv_init(21, 1)
    payload: [4]u8 = ipc.zero_payload()
    effect: service_effect.Effect

    payload[0] = 3
    payload[1] = 10
    effect = kv_service.handle(&state, service_effect.message(7, 90, 2, payload))
    if service_effect.effect_has_send(effect) == 0 {
        return false
    }

    payload[1] = 99
    effect = kv_service.handle(&state, service_effect.message(7, 90, 2, payload))
    if service_effect.effect_has_send(effect) == 0 {
        return false
    }

    effect = kv_service.handle(&state, service_effect.message(7, 90, 1, payload))
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
    effect: service_effect.Effect

    payload[0] = 1
    payload[1] = 10
    effect = kv_service.handle(&state, service_effect.message(7, 90, 2, payload))
    if service_effect.effect_has_send(effect) == 0 {
        return false
    }

    payload[0] = 2
    payload[1] = 20
    effect = kv_service.handle(&state, service_effect.message(7, 90, 2, payload))
    if service_effect.effect_has_send(effect) == 0 {
        return false
    }

    payload[0] = 1
    effect = kv_service.handle(&state, service_effect.message(7, 90, 1, payload))
    if service_effect.effect_reply_payload(effect)[1] != 10 {
        return false
    }

    payload[0] = 2
    effect = kv_service.handle(&state, service_effect.message(7, 90, 1, payload))
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
