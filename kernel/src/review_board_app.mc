import active_region
import display_surface
import journal_service
import review_board_ui

enum ReviewBoardPresentKind {
    NoChange,
    Presented,
}

const REVIEW_BOARD_JOURNAL_LANE: u8 = journal_service.JOURNAL_LANE_A
const REVIEW_BOARD_JOURNAL_STATE_LEN: usize = 4
const REVIEW_BOARD_MODE_AUDIT: u8 = 65
const REVIEW_BOARD_MODE_FOCUS: u8 = 70
const REVIEW_BOARD_JOURNAL_TAG_AUDIT_BODY: u8 = 65
const REVIEW_BOARD_JOURNAL_TAG_AUDIT_STATUS: u8 = 66
const REVIEW_BOARD_JOURNAL_TAG_FOCUS_BODY: u8 = 70
const REVIEW_BOARD_JOURNAL_TAG_FOCUS_STATUS: u8 = 71

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

func review_board_journal_tag(app: ReviewBoardAppState) u8 {
    if app.ui.mode == review_board_ui.ReviewBoardMode.Focus {
        if app.ui.focus == active_region.ActiveRegion.Status {
            return REVIEW_BOARD_JOURNAL_TAG_FOCUS_STATUS
        }
        return REVIEW_BOARD_JOURNAL_TAG_FOCUS_BODY
    }
    if app.ui.focus == active_region.ActiveRegion.Status {
        return REVIEW_BOARD_JOURNAL_TAG_AUDIT_STATUS
    }
    return REVIEW_BOARD_JOURNAL_TAG_AUDIT_BODY
}

func review_board_journal_state_bytes(app: ReviewBoardAppState) [journal_service.JOURNAL_RECORD_CAPACITY]u8 {
    bytes: [journal_service.JOURNAL_RECORD_CAPACITY]u8
    bytes[0] = app.ui.open
    bytes[1] = app.ui.closed
    bytes[2] = app.ui.urgent
    bytes[3] = review_board_journal_tag(app)
    return bytes
}

func review_board_journal_mode_from_state(tag: u8) review_board_ui.ReviewBoardMode {
    if tag == REVIEW_BOARD_JOURNAL_TAG_FOCUS_BODY || tag == REVIEW_BOARD_JOURNAL_TAG_FOCUS_STATUS {
        return review_board_ui.ReviewBoardMode.Focus
    }
    return review_board_ui.ReviewBoardMode.Audit
}

func review_board_journal_focus_from_state(tag: u8) active_region.ActiveRegion {
    if tag == REVIEW_BOARD_JOURNAL_TAG_AUDIT_STATUS || tag == REVIEW_BOARD_JOURNAL_TAG_FOCUS_STATUS {
        return active_region.ActiveRegion.Status
    }
    return active_region.ActiveRegion.Body
}

func review_board_journal_tag_valid(tag: u8) bool {
    return tag == REVIEW_BOARD_JOURNAL_TAG_AUDIT_BODY || tag == REVIEW_BOARD_JOURNAL_TAG_AUDIT_STATUS || tag == REVIEW_BOARD_JOURNAL_TAG_FOCUS_BODY || tag == REVIEW_BOARD_JOURNAL_TAG_FOCUS_STATUS
}

func review_board_app_reload(journal: journal_service.JournalServiceState) ReviewBoardAppState {
    lane := journal_service.lane_at(journal, REVIEW_BOARD_JOURNAL_LANE)
    if lane.len != REVIEW_BOARD_JOURNAL_STATE_LEN {
        return review_board_app_init()
    }
    if !review_board_journal_tag_valid(lane.data[3]) {
        return review_board_app_init()
    }
    return review_board_appwith(
        review_board_ui.review_board_uiwith(
            lane.data[0],
            lane.data[1],
            lane.data[2],
            review_board_journal_mode_from_state(lane.data[3]),
            review_board_journal_focus_from_state(lane.data[3])))
}

func review_board_sync_journal(journal: journal_service.JournalServiceState, app: ReviewBoardAppState) journal_service.JournalServiceState {
    return journal_service.journal_write_lane(journal, REVIEW_BOARD_JOURNAL_LANE, REVIEW_BOARD_JOURNAL_STATE_LEN, review_board_journal_state_bytes(app))
}

func review_board_present(app: ReviewBoardAppState, display: display_surface.DisplaySurfaceState) ReviewBoardPresentResult {
    result := display_surface.display_present(display, review_board_ui.review_board_ui_display_cells(app.ui))
    return review_board_result(app, result.state, ReviewBoardPresentKind.Presented)
}

func review_board_launch(app: ReviewBoardAppState, display: display_surface.DisplaySurfaceState) ReviewBoardPresentResult {
    return review_board_present(app, display)
}

func review_board_input(app: ReviewBoardAppState, display: display_surface.DisplaySurfaceState, key: u8) ReviewBoardPresentResult {
    next := review_board_ui.review_board_ui_handle_key(app.ui, key)
    if !next.changed {
        return review_board_result(app, display, ReviewBoardPresentKind.NoChange)
    }
    return review_board_present(review_board_appwith(next.state), display)
}