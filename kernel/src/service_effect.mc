import primitives
import syscall

struct Message {
    source_pid: u32
    endpoint_id: u32
    payload_len: usize
    payload: [4]u8
}

enum EffectKind {
    None,
    Reply,
    Send,
}

struct Effect {
    kind: EffectKind
    reply_status: syscall.SyscallStatus
    reply_payload_len: usize
    reply_payload: [4]u8
    send_source_pid: u32
    send_endpoint_id: u32
    send_payload_len: usize
    send_payload: [4]u8
    event_count: usize
    event0: u32
    event1: u32
    event2: u32
    event3: u32
    send_dropped: u32
}

func message(source_pid: u32, endpoint_id: u32, payload_len: usize, payload: [4]u8) Message {
    return Message{ source_pid: source_pid, endpoint_id: endpoint_id, payload_len: payload_len, payload: payload }
}

func effect_none() Effect {
    payload: [4]u8 = primitives.zero_payload()
    return Effect{ kind: EffectKind.None, reply_status: syscall.SyscallStatus.None, reply_payload_len: 0, reply_payload: payload, send_source_pid: 0, send_endpoint_id: 0, send_payload_len: 0, send_payload: payload, event_count: 0, event0: 0, event1: 0, event2: 0, event3: 0, send_dropped: 0 }
}

func effect_reply(status: syscall.SyscallStatus, payload_len: usize, payload: [4]u8) Effect {
    return Effect{ kind: EffectKind.Reply, reply_status: status, reply_payload_len: payload_len, reply_payload: payload, send_source_pid: 0, send_endpoint_id: 0, send_payload_len: 0, send_payload: primitives.zero_payload(), event_count: 0, event0: 0, event1: 0, event2: 0, event3: 0, send_dropped: 0 }
}

func effect_send(source_pid: u32, endpoint_id: u32, payload_len: usize, payload: [4]u8) Effect {
    return Effect{ kind: EffectKind.Send, reply_status: syscall.SyscallStatus.None, reply_payload_len: 0, reply_payload: primitives.zero_payload(), send_source_pid: source_pid, send_endpoint_id: endpoint_id, send_payload_len: payload_len, send_payload: payload, event_count: 0, event0: 0, event1: 0, event2: 0, event3: 0, send_dropped: 0 }
}

func effect_has_reply(effect: Effect) u32 {
    if effect.kind == EffectKind.Reply {
        return 1
    }
    return 0
}

func effect_has_send(effect: Effect) u32 {
    if effect.kind == EffectKind.Send {
        return 1
    }
    return 0
}

func effect_reply_status(effect: Effect) syscall.SyscallStatus {
    return effect.reply_status
}

func effect_reply_payload_len(effect: Effect) usize {
    return effect.reply_payload_len
}

func effect_reply_payload(effect: Effect) [4]u8 {
    return effect.reply_payload
}

func effect_send_endpoint_id(effect: Effect) u32 {
    return effect.send_endpoint_id
}

func effect_send_source_pid(effect: Effect) u32 {
    return effect.send_source_pid
}

func effect_send_payload_len(effect: Effect) usize {
    return effect.send_payload_len
}

func effect_send_payload(effect: Effect) [4]u8 {
    return effect.send_payload
}

func effect_event_count(effect: Effect) usize {
    return effect.event_count
}

func effect_event(effect: Effect, i: usize) u32 {
    if i >= effect.event_count {
        return 0
    }
    if i == 0 {
        return effect.event0
    }
    if i == 1 {
        return effect.event1
    }
    if i == 2 {
        return effect.event2
    }
    return effect.event3
}

func effectwith_events(e: Effect, count: usize, e0: u32, e1: u32, e2: u32, e3: u32) Effect {
    return Effect{ kind: e.kind, reply_status: e.reply_status, reply_payload_len: e.reply_payload_len, reply_payload: e.reply_payload, send_source_pid: e.send_source_pid, send_endpoint_id: e.send_endpoint_id, send_payload_len: e.send_payload_len, send_payload: e.send_payload, event_count: count, event0: e0, event1: e1, event2: e2, event3: e3, send_dropped: e.send_dropped }
}

func effect_with_event(effect: Effect, event_code: u32) Effect {
    if effect.event_count == 0 {
        return effectwith_events(effect, 1, event_code, 0, 0, 0)
    }
    if effect.event_count == 1 {
        return effectwith_events(effect, 2, effect.event0, event_code, 0, 0)
    }
    if effect.event_count == 2 {
        return effectwith_events(effect, 3, effect.event0, effect.event1, event_code, 0)
    }
    if effect.event_count == 3 {
        return effectwith_events(effect, 4, effect.event0, effect.event1, effect.event2, event_code)
    }
    return effect
}

func effect_mark_send_dropped(e: Effect) Effect {
    return Effect{ kind: e.kind, reply_status: e.reply_status, reply_payload_len: e.reply_payload_len, reply_payload: e.reply_payload, send_source_pid: e.send_source_pid, send_endpoint_id: e.send_endpoint_id, send_payload_len: e.send_payload_len, send_payload: e.send_payload, event_count: e.event_count, event0: e.event0, event1: e.event1, event2: e.event2, event3: e.event3, send_dropped: 1 }
}

func effect_send_dropped(e: Effect) u32 {
    return e.send_dropped
}

func from_receive_observation(obs: syscall.ReceiveObservation) Message {
    return message(obs.source_pid, obs.endpoint_id, obs.payload_len, obs.payload)
}