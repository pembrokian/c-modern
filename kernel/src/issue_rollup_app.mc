import display_surface
import program_catalog
import rollup_manifest
import rollup_model
import rollup_render
import update_store_service

enum IssueRollupPresentKind {
    NoChange,
    Presented,
}

struct IssueRollupAppState {
    summary: rollup_model.Summary
}

struct IssueRollupPresentResult {
    app: IssueRollupAppState
    display: display_surface.DisplaySurfaceState
    kind: IssueRollupPresentKind
}

func issue_rollup_app_init() IssueRollupAppState {
    return IssueRollupAppState{ summary: { open_items: 0, closed_items: 0, blocked_items: 0, priority_items: 0 } }
}

func issue_rollupwith(summary: rollup_model.Summary) IssueRollupAppState {
    return IssueRollupAppState{ summary: summary }
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
    cells := rollup_render.rollup_display_cells_with_manifest(app.summary, manifest)
    result := display_surface.display_present(display, cells)
    return issue_rollup_result(app, result.state, IssueRollupPresentKind.Presented)
}

func issue_rollup_launch(update_store: update_store_service.UpdateStoreServiceState, display: display_surface.DisplaySurfaceState) IssueRollupPresentResult {
    return issue_rollup_present(issue_rollup_app_init(), update_store, display)
}

func issue_rollup_summary_equal(left: rollup_model.Summary, right: rollup_model.Summary) bool {
    if left.open_items != right.open_items {
        return false
    }
    if left.closed_items != right.closed_items {
        return false
    }
    if left.blocked_items != right.blocked_items {
        return false
    }
    return left.priority_items == right.priority_items
}

func issue_rollup_summary_for_key(current: rollup_model.Summary, key: u8) rollup_model.Summary {
    switch key {
    case 69:
        return issue_rollup_app_init().summary
    case 83:
        return rollup_model.Summary{ open_items: 1, closed_items: 0, blocked_items: 0, priority_items: 0 }
    case 66:
        return rollup_model.Summary{ open_items: 2, closed_items: 0, blocked_items: 0, priority_items: 0 }
    case 65:
        return rollup_model.Summary{ open_items: 0, closed_items: 0, blocked_items: 2, priority_items: 0 }
    default:
        return current
    }
}

func issue_rollup_input(app: IssueRollupAppState, update_store: update_store_service.UpdateStoreServiceState, display: display_surface.DisplaySurfaceState, key: u8) IssueRollupPresentResult {
    next_summary := issue_rollup_summary_for_key(app.summary, key)
    if issue_rollup_summary_equal(next_summary, app.summary) {
        return issue_rollup_result(app, display, IssueRollupPresentKind.NoChange)
    }
    return issue_rollup_present(issue_rollupwith(next_summary), update_store, display)
}