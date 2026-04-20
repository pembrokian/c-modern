import display_surface
import review_board_ui

enum ReviewBoardPresentKind {
    NoChange,
    Presented,
}

struct ReviewBoardAppState {
    ui: review_board_ui.ReviewBoardUiState
}

struct ReviewBoardPresentResult {
    app: ReviewBoardAppState
    display: display_surface.DisplaySurfaceState
    kind: ReviewBoardPresentKind
}

func review_board_app_init() ReviewBoardAppState {
    return ReviewBoardAppState{ ui: review_board_ui.review_board_ui_init() }
}

func review_board_appwith(ui: review_board_ui.ReviewBoardUiState) ReviewBoardAppState {
    return ReviewBoardAppState{ ui: ui }
}

func review_board_result(app: ReviewBoardAppState, display: display_surface.DisplaySurfaceState, kind: ReviewBoardPresentKind) ReviewBoardPresentResult {
    return ReviewBoardPresentResult{ app: app, display: display, kind: kind }
}

func review_board_present(app: ReviewBoardAppState, display: display_surface.DisplaySurfaceState) ReviewBoardPresentResult {
    result := display_surface.display_present(display, review_board_ui.review_board_ui_display_cells(app.ui))
    return review_board_result(app, result.state, ReviewBoardPresentKind.Presented)
}

func review_board_launch(display: display_surface.DisplaySurfaceState) ReviewBoardPresentResult {
    return review_board_present(review_board_app_init(), display)
}

func review_board_input(app: ReviewBoardAppState, display: display_surface.DisplaySurfaceState, key: u8) ReviewBoardPresentResult {
    next := review_board_ui.review_board_ui_handle_key(app.ui, key)
    if !next.changed {
        return review_board_result(app, display, ReviewBoardPresentKind.NoChange)
    }
    return review_board_present(review_board_appwith(next.state), display)
}