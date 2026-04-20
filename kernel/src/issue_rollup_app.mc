import display_surface
import issue_rollup_interactive
import program_catalog
import rollup_manifest
import rollup_render
import surface_composition
import update_store_service

enum IssueRollupPresentKind {
    NoChange,
    Presented,
}

enum IssueRollupFocus {
    Body,
    Status,
}

const ISSUE_ROLLUP_LAUNCH_STATUS_NONE: u8 = 0
const ISSUE_ROLLUP_KEY_FOCUS: u8 = 70

struct IssueRollupAppState {
    interactive: issue_rollup_interactive.IssueRollupInteractiveState
    focus: IssueRollupFocus
    launch_status: u8
}

struct IssueRollupPresentResult {
    app: IssueRollupAppState
    display: display_surface.DisplaySurfaceState
    kind: IssueRollupPresentKind
}

func issue_rollup_app_init(launch_status: u8) IssueRollupAppState {
    return IssueRollupAppState{
        interactive: issue_rollup_interactive.issue_rollup_interactive_init(),
        focus: IssueRollupFocus.Body,
        launch_status: launch_status }
}

func issue_rollupwith(interactive: issue_rollup_interactive.IssueRollupInteractiveState, focus: IssueRollupFocus, launch_status: u8) IssueRollupAppState {
    return IssueRollupAppState{ interactive: interactive, focus: focus, launch_status: launch_status }
}

func issue_rollup_result(app: IssueRollupAppState, display: display_surface.DisplaySurfaceState, kind: IssueRollupPresentKind) IssueRollupPresentResult {
    return IssueRollupPresentResult{ app: app, display: display, kind: kind }
}

func issue_rollup_region_head(cells: [display_surface.DISPLAY_CELL_COUNT]u8) [surface_composition.SURFACE_REGION_CELL_COUNT]u8 {
    return [surface_composition.SURFACE_REGION_CELL_COUNT]u8{ cells[0], cells[1] }
}

func issue_rollup_manifest_is_custom(manifest: rollup_manifest.RollupManifest) bool {
    default_manifest := rollup_manifest.default_manifest()
    if manifest.busy_open_floor != default_manifest.busy_open_floor {
        return true
    }
    if manifest.blocked_attention_floor != default_manifest.blocked_attention_floor {
        return true
    }
    return manifest.priority_bonus != default_manifest.priority_bonus
}

func issue_rollup_manifest_strip(manifest: rollup_manifest.RollupManifest) [surface_composition.SURFACE_REGION_CELL_COUNT]u8 {
    if issue_rollup_manifest_is_custom(manifest) {
        return [surface_composition.SURFACE_REGION_CELL_COUNT]u8{ 67, 70 }
    }
    return [surface_composition.SURFACE_REGION_CELL_COUNT]u8{ 68, 70 }
}

func issue_rollup_focus_next(focus: IssueRollupFocus) IssueRollupFocus {
    switch focus {
    case IssueRollupFocus.Status:
        return IssueRollupFocus.Body
    default:
        return IssueRollupFocus.Status
    }
}

func issue_rollup_focus_strip(manifest: rollup_manifest.RollupManifest, focus: IssueRollupFocus) [surface_composition.SURFACE_REGION_CELL_COUNT]u8 {
    if !issue_rollup_manifest_is_custom(manifest) {
        return issue_rollup_manifest_strip(manifest)
    }

    switch focus {
    case IssueRollupFocus.Status:
        return [surface_composition.SURFACE_REGION_CELL_COUNT]u8{ 67, 83 }
    default:
        return [surface_composition.SURFACE_REGION_CELL_COUNT]u8{ 67, 70 }
    }
}

func issue_rollup_composed_present(display: display_surface.DisplaySurfaceState, cells: [display_surface.DISPLAY_CELL_COUNT]u8, manifest: rollup_manifest.RollupManifest) display_surface.DisplayResult {
    return surface_composition.compose_present(
        display,
        surface_composition.surface_composition(
            issue_rollup_region_head(cells),
            issue_rollup_focus_strip(manifest, IssueRollupFocus.Body)))
}

func issue_rollup_focus_present(display: display_surface.DisplaySurfaceState, cells: [display_surface.DISPLAY_CELL_COUNT]u8, manifest: rollup_manifest.RollupManifest, focus: IssueRollupFocus) display_surface.DisplayResult {
    return surface_composition.compose_present(
        display,
        surface_composition.surface_composition(
            issue_rollup_region_head(cells),
            issue_rollup_focus_strip(manifest, focus)))
}

func issue_rollup_manifest(update_store: update_store_service.UpdateStoreServiceState) rollup_manifest.RollupManifest {
    if !update_store_service.update_installed_present(update_store) {
        return rollup_manifest.default_manifest()
    }
    if update_store_service.update_installed_program_id(update_store) != program_catalog.PROGRAM_ID_ISSUE_ROLLUP {
        return rollup_manifest.default_manifest()
    }
    return rollup_manifest.manifest_decode(
        update_store_service.update_installed_version(update_store),
        update_store_service.update_installed_len(update_store),
        update_store_service.update_installed_byte(update_store, 0),
        update_store_service.update_installed_byte(update_store, 1),
        update_store_service.update_installed_byte(update_store, 2))
}

func issue_rollup_present(app: IssueRollupAppState, update_store: update_store_service.UpdateStoreServiceState, display: display_surface.DisplaySurfaceState) IssueRollupPresentResult {
    if app.launch_status != ISSUE_ROLLUP_LAUNCH_STATUS_NONE {
        result := display_surface.display_present(display, rollup_render.rollup_display_cells_for_launch_status(app.launch_status))
        return issue_rollup_result(app, result.state, IssueRollupPresentKind.Presented)
    }

    manifest := issue_rollup_manifest(update_store)
    summary := issue_rollup_interactive.issue_rollup_summary(app.interactive)
    cells := rollup_render.rollup_display_cells_with_manifest(summary, manifest)
    result := display_surface.display_present(display, cells)
    if issue_rollup_manifest_is_custom(manifest) {
        result = issue_rollup_focus_present(display, cells, manifest, app.focus)
    }
    return issue_rollup_result(app, result.state, IssueRollupPresentKind.Presented)
}

func issue_rollup_launch(launch_status: u8, update_store: update_store_service.UpdateStoreServiceState, display: display_surface.DisplaySurfaceState) IssueRollupPresentResult {
    return issue_rollup_present(issue_rollup_app_init(launch_status), update_store, display)
}

func issue_rollup_input(app: IssueRollupAppState, update_store: update_store_service.UpdateStoreServiceState, display: display_surface.DisplaySurfaceState, key: u8) IssueRollupPresentResult {
    manifest := issue_rollup_manifest(update_store)
    if key == ISSUE_ROLLUP_KEY_FOCUS && issue_rollup_manifest_is_custom(manifest) {
        return issue_rollup_present(
            issue_rollupwith(app.interactive, issue_rollup_focus_next(app.focus), ISSUE_ROLLUP_LAUNCH_STATUS_NONE),
            update_store,
            display)
    }

    if app.focus == IssueRollupFocus.Status {
        return issue_rollup_result(app, display, IssueRollupPresentKind.NoChange)
    }

    next := issue_rollup_interactive.issue_rollup_handle_key(app.interactive, key)
    if !next.changed {
        return issue_rollup_result(app, display, IssueRollupPresentKind.NoChange)
    }
    return issue_rollup_present(issue_rollupwith(next.state, app.focus, ISSUE_ROLLUP_LAUNCH_STATUS_NONE), update_store, display)
}