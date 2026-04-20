import display_surface
import issue_rollup_interactive
import program_catalog
import rollup_manifest
import rollup_render
import update_store_service

enum IssueRollupPresentKind {
    NoChange,
    Presented,
}

struct IssueRollupAppState {
    interactive: issue_rollup_interactive.IssueRollupInteractiveState
}

struct IssueRollupPresentResult {
    app: IssueRollupAppState
    display: display_surface.DisplaySurfaceState
    kind: IssueRollupPresentKind
}

func issue_rollup_app_init() IssueRollupAppState {
    return IssueRollupAppState{ interactive: issue_rollup_interactive.issue_rollup_interactive_init() }
}

func issue_rollupwith(interactive: issue_rollup_interactive.IssueRollupInteractiveState) IssueRollupAppState {
    return IssueRollupAppState{ interactive: interactive }
}

func issue_rollup_result(app: IssueRollupAppState, display: display_surface.DisplaySurfaceState, kind: IssueRollupPresentKind) IssueRollupPresentResult {
    return IssueRollupPresentResult{ app: app, display: display, kind: kind }
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
    manifest := issue_rollup_manifest(update_store)
    summary := issue_rollup_interactive.issue_rollup_summary(app.interactive)
    cells := rollup_render.rollup_display_cells_with_manifest(summary, manifest)
    result := display_surface.display_present(display, cells)
    return issue_rollup_result(app, result.state, IssueRollupPresentKind.Presented)
}

func issue_rollup_launch(update_store: update_store_service.UpdateStoreServiceState, display: display_surface.DisplaySurfaceState) IssueRollupPresentResult {
    return issue_rollup_present(issue_rollup_app_init(), update_store, display)
}

func issue_rollup_input(app: IssueRollupAppState, update_store: update_store_service.UpdateStoreServiceState, display: display_surface.DisplaySurfaceState, key: u8) IssueRollupPresentResult {
    next := issue_rollup_interactive.issue_rollup_handle_key(app.interactive, key)
    if !next.changed {
        return issue_rollup_result(app, display, IssueRollupPresentKind.NoChange)
    }
    return issue_rollup_present(issue_rollupwith(next.state), update_store, display)
}