import display_surface
import input_event
import issue_rollup_app
import launcher_service
import program_catalog
import service_effect
import update_store_service

enum ForegroundInputRouteKind {
    NotInput,
    Delivered,
    NoForeground,
    Unsupported,
}

struct ForegroundInputRoute {
    kind: ForegroundInputRouteKind
    issue_rollup: issue_rollup_app.IssueRollupAppState
    display: display_surface.DisplaySurfaceState
    effect: service_effect.Effect
}

func foreground_input_route(kind: ForegroundInputRouteKind, issue_rollup: issue_rollup_app.IssueRollupAppState, display: display_surface.DisplaySurfaceState, effect: service_effect.Effect) ForegroundInputRoute {
    return ForegroundInputRoute{ kind: kind, issue_rollup: issue_rollup, display: display, effect: effect }
}

func no_foreground_reply(issue_rollup: issue_rollup_app.IssueRollupAppState, display: display_surface.DisplaySurfaceState, parsed: input_event.InputParse) ForegroundInputRoute {
    return foreground_input_route(
        ForegroundInputRouteKind.NoForeground,
        issue_rollup,
        display,
        input_event.input_reply(program_catalog.PROGRAM_ID_NONE, input_event.INPUT_ROUTE_NO_FOREGROUND, parsed.event, parsed.value))
}

func issue_rollup_reply(issue_rollup: issue_rollup_app.IssueRollupAppState, update_store: update_store_service.UpdateStoreServiceState, display: display_surface.DisplaySurfaceState, parsed: input_event.InputParse) ForegroundInputRoute {
    if parsed.kind != input_event.InputParseKind.Key {
        return foreground_input_route(
            ForegroundInputRouteKind.Unsupported,
            issue_rollup,
            display,
            input_event.input_reply(program_catalog.PROGRAM_ID_ISSUE_ROLLUP, input_event.INPUT_ROUTE_UNSUPPORTED, parsed.event, parsed.value))
    }
    present := issue_rollup_app.issue_rollup_input(issue_rollup, update_store, display, parsed.value)
    return foreground_input_route(
        ForegroundInputRouteKind.Delivered,
        present.app,
        present.display,
        input_event.input_reply(program_catalog.PROGRAM_ID_ISSUE_ROLLUP, input_event.INPUT_ROUTE_DELIVERED, parsed.event, parsed.value))
}

func unsupported_reply(target: u8, issue_rollup: issue_rollup_app.IssueRollupAppState, display: display_surface.DisplaySurfaceState, parsed: input_event.InputParse) ForegroundInputRoute {
    return foreground_input_route(
        ForegroundInputRouteKind.Unsupported,
        issue_rollup,
        display,
        input_event.input_reply(target, input_event.INPUT_ROUTE_UNSUPPORTED, parsed.event, parsed.value))
}

func handle(launcher: launcher_service.LauncherServiceState, issue_rollup: issue_rollup_app.IssueRollupAppState, update_store: update_store_service.UpdateStoreServiceState, display: display_surface.DisplaySurfaceState, payload_len: usize, payload: [4]u8) ForegroundInputRoute {
    parsed := input_event.handle(payload_len, payload)
    if parsed.kind == input_event.InputParseKind.NotInput {
        return foreground_input_route(ForegroundInputRouteKind.NotInput, issue_rollup, display, service_effect.effect_none())
    }

    foreground := launcher_service.launcher_foreground_id(launcher)
    if foreground == program_catalog.PROGRAM_ID_NONE {
        return no_foreground_reply(issue_rollup, display, parsed)
    }

    switch foreground {
    case program_catalog.PROGRAM_ID_ISSUE_ROLLUP:
        return issue_rollup_reply(issue_rollup, update_store, display, parsed)
    default:
        return unsupported_reply(foreground, issue_rollup, display, parsed)
    }
}