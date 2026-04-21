import primitives
import serial_protocol
import service_effect
import syscall

enum InputParseKind {
    NotInput,
    Key,
    Unsupported,
}

struct InputParse {
    kind: InputParseKind
    event: u8
    value: u8
}

const INPUT_ROUTE_DELIVERED: u8 = 'D'
const INPUT_ROUTE_NO_FOREGROUND: u8 = 'N'
const INPUT_ROUTE_UNSUPPORTED: u8 = 'U'
const INPUT_EVENT_KEY: u8 = 'K'

func input_parse(kind: InputParseKind, event: u8, value: u8) InputParse {
    return InputParse{ kind: kind, event: event, value: value }
}

func input_reply(target: u8, route: u8, event: u8, value: u8) service_effect.Effect {
    payload := primitives.zero_payload()
    payload[0] = target
    payload[1] = route
    payload[2] = event
    payload[3] = value
    return service_effect.effect_reply(syscall.SyscallStatus.Ok, 4, payload)
}

func input_is_frame(payload_len: usize, payload: [4]u8) bool {
    return payload_len == 4 && payload[0] == serial_protocol.CMD_I
}

func handle(payload_len: usize, payload: [4]u8) InputParse {
    if !input_is_frame(payload_len, payload) {
        return input_parse(InputParseKind.NotInput, 0, 0)
    }
    if payload[1] == INPUT_EVENT_KEY && payload[3] == serial_protocol.CMD_BANG {
        return input_parse(InputParseKind.Key, INPUT_EVENT_KEY, payload[2])
    }
    return input_parse(InputParseKind.Unsupported, payload[1], payload[2])
}