import boot
import display_surface
import input_event
import issue_rollup_app
import launcher_service
import program_catalog
import review_board_app
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
    apps: boot.AppRuntimeState
    display: display_surface.DisplaySurfaceState
    effect: service_effect.Effect
}

func foreground_input_route(kind: ForegroundInputRouteKind, apps: boot.AppRuntimeState, display: display_surface.DisplaySurfaceState, effect: service_effect.Effect) ForegroundInputRoute {
    return ForegroundInputRoute{ kind: kind, apps: apps, display: display, effect: effect }
}

func no_foreground_reply(apps: boot.AppRuntimeState, display: display_surface.DisplaySurfaceState, parsed: input_event.InputParse) ForegroundInputRoute {
    return foreground_input_route(
        ForegroundInputRouteKind.NoForeground,
        apps,
        display,
        input_event.input_reply(program_catalog.PROGRAM_ID_NONE, input_event.INPUT_ROUTE_NO_FOREGROUND, parsed.event, parsed.value))
}

func issue_rollup_reply(apps: boot.AppRuntimeState, update_store: update_store_service.UpdateStoreServiceState, display: display_surface.DisplaySurfaceState, parsed: input_event.InputParse) ForegroundInputRoute {
    if parsed.kind != input_event.InputParseKind.Key {
        return foreground_input_route(
            ForegroundInputRouteKind.Unsupported,
            apps,
            display,
            input_event.input_reply(program_catalog.PROGRAM_ID_ISSUE_ROLLUP, input_event.INPUT_ROUTE_UNSUPPORTED, parsed.event, parsed.value))
    }
    present := issue_rollup_app.issue_rollup_input(apps.issue_rollup, update_store, display, parsed.value)
    return foreground_input_route(
        ForegroundInputRouteKind.Delivered,
        boot.app_runtime_state(present.app, apps.review_board),
        present.display,
        input_event.input_reply(program_catalog.PROGRAM_ID_ISSUE_ROLLUP, input_event.INPUT_ROUTE_DELIVERED, parsed.event, parsed.value))
}

func review_board_reply(apps: boot.AppRuntimeState, display: display_surface.DisplaySurfaceState, parsed: input_event.InputParse) ForegroundInputRoute {
    if parsed.kind != input_event.InputParseKind.Key {
        return foreground_input_route(
            ForegroundInputRouteKind.Unsupported,
            apps,
            display,
            input_event.input_reply(program_catalog.PROGRAM_ID_REVIEW_BOARD, input_event.INPUT_ROUTE_UNSUPPORTED, parsed.event, parsed.value))
    }
    present := review_board_app.review_board_input(apps.review_board, display, parsed.value)
    return foreground_input_route(
        ForegroundInputRouteKind.Delivered,
        boot.app_runtime_state(apps.issue_rollup, present.app),
        present.display,
        input_event.input_reply(program_catalog.PROGRAM_ID_REVIEW_BOARD, input_event.INPUT_ROUTE_DELIVERED, parsed.event, parsed.value))
}

func unsupported_reply(target: u8, apps: boot.AppRuntimeState, display: display_surface.DisplaySurfaceState, parsed: input_event.InputParse) ForegroundInputRoute {
    return foreground_input_route(
        ForegroundInputRouteKind.Unsupported,
        apps,
        display,
        input_event.input_reply(target, input_event.INPUT_ROUTE_UNSUPPORTED, parsed.event, parsed.value))
}

func handle(launcher: launcher_service.LauncherServiceState, apps: boot.AppRuntimeState, update_store: update_store_service.UpdateStoreServiceState, display: display_surface.DisplaySurfaceState, payload_len: usize, payload: [4]u8) ForegroundInputRoute {
    parsed := input_event.handle(payload_len, payload)
    if parsed.kind == input_event.InputParseKind.NotInput {
        return foreground_input_route(ForegroundInputRouteKind.NotInput, apps, display, service_effect.effect_none())
    }

    foreground := launcher_service.launcher_foreground_id(launcher)
    if foreground == program_catalog.PROGRAM_ID_NONE {
        return no_foreground_reply(apps, display, parsed)
    }

    switch foreground {
    case program_catalog.PROGRAM_ID_ISSUE_ROLLUP:
        return issue_rollup_reply(apps, update_store, display, parsed)
    case program_catalog.PROGRAM_ID_REVIEW_BOARD:
        return review_board_reply(apps, display, parsed)
    default:
        return unsupported_reply(foreground, apps, display, parsed)
    }
}