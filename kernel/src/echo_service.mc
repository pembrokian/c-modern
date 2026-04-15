import primitives
import service_effect
import syscall

const ECHO_CAPACITY: usize = 4

struct EchoServiceState {
    pid: u32
    slot: u32
    count: usize
}

struct EchoResult {
    state: EchoServiceState
    effect: service_effect.Effect
}

func echo_init(pid: u32, slot: u32) EchoServiceState {
    return EchoServiceState{ pid: pid, slot: slot, count: 0 }
}

func echowith(s: EchoServiceState, count: usize) EchoServiceState {
    return EchoServiceState{ pid: s.pid, slot: s.slot, count: count }
}

func echoreply(m: service_effect.Message) service_effect.Effect {
    reply: [4]u8 = primitives.zero_payload()
    for i in 0..m.payload_len {
        reply[i] = m.payload[i]
    }
    return service_effect.effect_reply(syscall.SyscallStatus.Ok, m.payload_len, reply)
}

func handle(s: EchoServiceState, m: service_effect.Message) EchoResult {
    if m.payload_len != 2 {
        return EchoResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.InvalidArgument, 0, primitives.zero_payload()) }
    }
    if s.count >= ECHO_CAPACITY {
        return EchoResult{ state: s, effect: service_effect.effect_reply(syscall.SyscallStatus.Exhausted, 0, primitives.zero_payload()) }
    }
    return EchoResult{ state: echowith(s, s.count + 1), effect: echoreply(m) }
}

func echo_count(s: EchoServiceState) usize {
    return s.count
}