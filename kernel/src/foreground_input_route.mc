import input_event
import launcher_service
import program_catalog
import service_effect

enum ForegroundInputRouteKind {
    NotInput,
    Delivered,
    NoForeground,
    Unsupported,
}

struct ForegroundInputRoute {
    kind: ForegroundInputRouteKind
    effect: service_effect.Effect
}

func foreground_input_route(kind: ForegroundInputRouteKind, effect: service_effect.Effect) ForegroundInputRoute {
    return ForegroundInputRoute{ kind: kind, effect: effect }
}

func no_foreground_reply(parsed: input_event.InputParse) ForegroundInputRoute {
    return foreground_input_route(
        ForegroundInputRouteKind.NoForeground,
        input_event.input_reply(program_catalog.PROGRAM_ID_NONE, input_event.INPUT_ROUTE_NO_FOREGROUND, parsed.event, parsed.value))
}

func issue_rollup_reply(parsed: input_event.InputParse) ForegroundInputRoute {
    if parsed.kind != input_event.InputParseKind.Key {
        return foreground_input_route(
            ForegroundInputRouteKind.Unsupported,
            input_event.input_reply(program_catalog.PROGRAM_ID_ISSUE_ROLLUP, input_event.INPUT_ROUTE_UNSUPPORTED, parsed.event, parsed.value))
    }
    return foreground_input_route(
        ForegroundInputRouteKind.Delivered,
        input_event.input_reply(program_catalog.PROGRAM_ID_ISSUE_ROLLUP, input_event.INPUT_ROUTE_DELIVERED, parsed.event, parsed.value))
}

func unsupported_reply(target: u8, parsed: input_event.InputParse) ForegroundInputRoute {
    return foreground_input_route(
        ForegroundInputRouteKind.Unsupported,
        input_event.input_reply(target, input_event.INPUT_ROUTE_UNSUPPORTED, parsed.event, parsed.value))
}

func handle(launcher: launcher_service.LauncherServiceState, payload_len: usize, payload: [4]u8) ForegroundInputRoute {
    parsed := input_event.handle(payload_len, payload)
    if parsed.kind == input_event.InputParseKind.NotInput {
        return foreground_input_route(ForegroundInputRouteKind.NotInput, service_effect.effect_none())
    }

    foreground := launcher_service.launcher_foreground_id(launcher)
    if foreground == program_catalog.PROGRAM_ID_NONE {
        return no_foreground_reply(parsed)
    }

    switch foreground {
    case program_catalog.PROGRAM_ID_ISSUE_ROLLUP:
        return issue_rollup_reply(parsed)
    default:
        return unsupported_reply(foreground, parsed)
    }
}