import primitives
import program_catalog
import serial_protocol
import service_effect
import syscall

enum InputRouteKind {
    NotInput,
    Delivered,
    NoForeground,
    Unsupported,
}

struct InputRoute {
    kind: InputRouteKind
    effect: service_effect.Effect
}

const INPUT_ROUTE_DELIVERED: u8 = 68
const INPUT_ROUTE_NO_FOREGROUND: u8 = 78
const INPUT_ROUTE_UNSUPPORTED: u8 = 85
const INPUT_EVENT_KEY: u8 = 75

func input_route(kind: InputRouteKind, effect: service_effect.Effect) InputRoute {
    return InputRoute{ kind: kind, effect: effect }
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

func input_key(payload: [4]u8) InputRoute {
    if payload[3] != serial_protocol.CMD_BANG {
        return input_route(InputRouteKind.NotInput, service_effect.effect_none())
    }
    return input_route(InputRouteKind.Delivered, input_reply(0, 0, INPUT_EVENT_KEY, payload[2]))
}

func issue_rollup_input(payload: [4]u8) InputRoute {
    if payload[1] != INPUT_EVENT_KEY {
        return input_route(InputRouteKind.Unsupported, input_reply(program_catalog.PROGRAM_ID_ISSUE_ROLLUP, INPUT_ROUTE_UNSUPPORTED, payload[1], payload[2]))
    }
    if payload[3] != serial_protocol.CMD_BANG {
        return input_route(InputRouteKind.Unsupported, input_reply(program_catalog.PROGRAM_ID_ISSUE_ROLLUP, INPUT_ROUTE_UNSUPPORTED, payload[1], payload[2]))
    }
    return input_route(InputRouteKind.Delivered, input_reply(program_catalog.PROGRAM_ID_ISSUE_ROLLUP, INPUT_ROUTE_DELIVERED, INPUT_EVENT_KEY, payload[2]))
}

func handle(foreground: u8, payload_len: usize, payload: [4]u8) InputRoute {
    if !input_is_frame(payload_len, payload) {
        return input_route(InputRouteKind.NotInput, service_effect.effect_none())
    }

    if foreground == program_catalog.PROGRAM_ID_NONE {
        if payload[1] == INPUT_EVENT_KEY && payload[3] == serial_protocol.CMD_BANG {
            return input_route(InputRouteKind.NoForeground, input_reply(program_catalog.PROGRAM_ID_NONE, INPUT_ROUTE_NO_FOREGROUND, INPUT_EVENT_KEY, payload[2]))
        }
        return input_route(InputRouteKind.NoForeground, input_reply(program_catalog.PROGRAM_ID_NONE, INPUT_ROUTE_NO_FOREGROUND, payload[1], payload[2]))
    }

    switch foreground {
    case program_catalog.PROGRAM_ID_ISSUE_ROLLUP:
        return issue_rollup_input(payload)
    default:
        return input_route(InputRouteKind.Unsupported, input_reply(foreground, INPUT_ROUTE_UNSUPPORTED, payload[1], payload[2]))
    }
}