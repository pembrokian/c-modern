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
const REVIEW_BOARD_JOURNAL_GROUP_AUDIT_BODY: u8 = 0
const REVIEW_BOARD_JOURNAL_GROUP_AUDIT_STATUS: u8 = 24
const REVIEW_BOARD_JOURNAL_GROUP_FOCUS_BODY: u8 = 48
const REVIEW_BOARD_JOURNAL_GROUP_FOCUS_STATUS: u8 = 72
const REVIEW_BOARD_JOURNAL_LOCAL_SUMMARY: u8 = 0
const REVIEW_BOARD_JOURNAL_LOCAL_OPEN: u8 = 1
const REVIEW_BOARD_JOURNAL_LOCAL_CLOSED: u8 = 2
const REVIEW_BOARD_JOURNAL_LOCAL_URGENT: u8 = 3
const REVIEW_BOARD_JOURNAL_LOCAL_FILTER_PROMPT_SUMMARY: u8 = 4
const REVIEW_BOARD_JOURNAL_LOCAL_FILTER_PROMPT_OPEN: u8 = 5
const REVIEW_BOARD_JOURNAL_LOCAL_FILTER_PROMPT_CLOSED: u8 = 6
const REVIEW_BOARD_JOURNAL_LOCAL_FILTER_PROMPT_URGENT: u8 = 7
const REVIEW_BOARD_JOURNAL_LOCAL_FILTER_FIRST_SUMMARY_AUDIT: u8 = 8
const REVIEW_BOARD_JOURNAL_LOCAL_FILTER_FIRST_SUMMARY_CLOSED: u8 = 9
const REVIEW_BOARD_JOURNAL_LOCAL_FILTER_FIRST_SUMMARY_OPEN: u8 = 10
const REVIEW_BOARD_JOURNAL_LOCAL_FILTER_FIRST_SUMMARY_URGENT: u8 = 11
const REVIEW_BOARD_JOURNAL_LOCAL_FILTER_FIRST_OPEN_AUDIT: u8 = 12
const REVIEW_BOARD_JOURNAL_LOCAL_FILTER_FIRST_OPEN_CLOSED: u8 = 13
const REVIEW_BOARD_JOURNAL_LOCAL_FILTER_FIRST_OPEN_OPEN: u8 = 14
const REVIEW_BOARD_JOURNAL_LOCAL_FILTER_FIRST_OPEN_URGENT: u8 = 15
const REVIEW_BOARD_JOURNAL_LOCAL_FILTER_FIRST_CLOSED_AUDIT: u8 = 16
const REVIEW_BOARD_JOURNAL_LOCAL_FILTER_FIRST_CLOSED_CLOSED: u8 = 17
const REVIEW_BOARD_JOURNAL_LOCAL_FILTER_FIRST_CLOSED_OPEN: u8 = 18
const REVIEW_BOARD_JOURNAL_LOCAL_FILTER_FIRST_CLOSED_URGENT: u8 = 19
const REVIEW_BOARD_JOURNAL_LOCAL_FILTER_FIRST_URGENT_AUDIT: u8 = 20
const REVIEW_BOARD_JOURNAL_LOCAL_FILTER_FIRST_URGENT_CLOSED: u8 = 21
const REVIEW_BOARD_JOURNAL_LOCAL_FILTER_FIRST_URGENT_OPEN: u8 = 22
const REVIEW_BOARD_JOURNAL_LOCAL_FILTER_FIRST_URGENT_URGENT: u8 = 23
const REVIEW_BOARD_JOURNAL_TAG_LIMIT: u8 = 96

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

func review_board_journal_group(mode: review_board_ui.ReviewBoardMode, focus: active_region.ActiveRegion) u8 {
    if mode == review_board_ui.ReviewBoardMode.Focus {
        if focus == active_region.ActiveRegion.Status {
            return REVIEW_BOARD_JOURNAL_GROUP_FOCUS_STATUS
        }
        return REVIEW_BOARD_JOURNAL_GROUP_FOCUS_BODY
    }
    if focus == active_region.ActiveRegion.Status {
        return REVIEW_BOARD_JOURNAL_GROUP_AUDIT_STATUS
    }
    return REVIEW_BOARD_JOURNAL_GROUP_AUDIT_BODY
}

func review_board_journal_detail_local(detail: review_board_ui.ReviewBoardDetail) u8 {
    switch detail {
    case review_board_ui.ReviewBoardDetail.Open:
        return REVIEW_BOARD_JOURNAL_LOCAL_OPEN
    case review_board_ui.ReviewBoardDetail.Closed:
        return REVIEW_BOARD_JOURNAL_LOCAL_CLOSED
    case review_board_ui.ReviewBoardDetail.Urgent:
        return REVIEW_BOARD_JOURNAL_LOCAL_URGENT
    default:
        return REVIEW_BOARD_JOURNAL_LOCAL_SUMMARY
    }
}

func review_board_journal_prompt_local(detail: review_board_ui.ReviewBoardDetail) u8 {
    switch detail {
    case review_board_ui.ReviewBoardDetail.Open:
        return REVIEW_BOARD_JOURNAL_LOCAL_FILTER_PROMPT_OPEN
    case review_board_ui.ReviewBoardDetail.Closed:
        return REVIEW_BOARD_JOURNAL_LOCAL_FILTER_PROMPT_CLOSED
    case review_board_ui.ReviewBoardDetail.Urgent:
        return REVIEW_BOARD_JOURNAL_LOCAL_FILTER_PROMPT_URGENT
    default:
        return REVIEW_BOARD_JOURNAL_LOCAL_FILTER_PROMPT_SUMMARY
    }
}

func review_board_journal_filter_first_local(detail: review_board_ui.ReviewBoardDetail, key: u8) u8 {
    switch detail {
    case review_board_ui.ReviewBoardDetail.Open:
        if key == review_board_ui.REVIEW_BOARD_KEY_AUDIT {
            return REVIEW_BOARD_JOURNAL_LOCAL_FILTER_FIRST_OPEN_AUDIT
        }
        if key == review_board_ui.REVIEW_BOARD_KEY_CLOSED {
            return REVIEW_BOARD_JOURNAL_LOCAL_FILTER_FIRST_OPEN_CLOSED
        }
        if key == review_board_ui.REVIEW_BOARD_KEY_OPEN {
            return REVIEW_BOARD_JOURNAL_LOCAL_FILTER_FIRST_OPEN_OPEN
        }
        return REVIEW_BOARD_JOURNAL_LOCAL_FILTER_FIRST_OPEN_URGENT
    case review_board_ui.ReviewBoardDetail.Closed:
        if key == review_board_ui.REVIEW_BOARD_KEY_AUDIT {
            return REVIEW_BOARD_JOURNAL_LOCAL_FILTER_FIRST_CLOSED_AUDIT
        }
        if key == review_board_ui.REVIEW_BOARD_KEY_CLOSED {
            return REVIEW_BOARD_JOURNAL_LOCAL_FILTER_FIRST_CLOSED_CLOSED
        }
        if key == review_board_ui.REVIEW_BOARD_KEY_OPEN {
            return REVIEW_BOARD_JOURNAL_LOCAL_FILTER_FIRST_CLOSED_OPEN
        }
        return REVIEW_BOARD_JOURNAL_LOCAL_FILTER_FIRST_CLOSED_URGENT
    case review_board_ui.ReviewBoardDetail.Urgent:
        if key == review_board_ui.REVIEW_BOARD_KEY_AUDIT {
            return REVIEW_BOARD_JOURNAL_LOCAL_FILTER_FIRST_URGENT_AUDIT
        }
        if key == review_board_ui.REVIEW_BOARD_KEY_CLOSED {
            return REVIEW_BOARD_JOURNAL_LOCAL_FILTER_FIRST_URGENT_CLOSED
        }
        if key == review_board_ui.REVIEW_BOARD_KEY_OPEN {
            return REVIEW_BOARD_JOURNAL_LOCAL_FILTER_FIRST_URGENT_OPEN
        }
        return REVIEW_BOARD_JOURNAL_LOCAL_FILTER_FIRST_URGENT_URGENT
    default:
        if key == review_board_ui.REVIEW_BOARD_KEY_AUDIT {
            return REVIEW_BOARD_JOURNAL_LOCAL_FILTER_FIRST_SUMMARY_AUDIT
        }
        if key == review_board_ui.REVIEW_BOARD_KEY_CLOSED {
            return REVIEW_BOARD_JOURNAL_LOCAL_FILTER_FIRST_SUMMARY_CLOSED
        }
        if key == review_board_ui.REVIEW_BOARD_KEY_OPEN {
            return REVIEW_BOARD_JOURNAL_LOCAL_FILTER_FIRST_SUMMARY_OPEN
        }
        return REVIEW_BOARD_JOURNAL_LOCAL_FILTER_FIRST_SUMMARY_URGENT
    }
}

func review_board_journal_local(ui: review_board_ui.ReviewBoardUiState) u8 {
    if !ui.filtering {
        return review_board_journal_detail_local(ui.detail)
    }
    if ui.filterlen == 0 {
        return review_board_journal_prompt_local(ui.detail)
    }
    return review_board_journal_filter_first_local(ui.detail, ui.filter0)
}

func review_board_journal_tag(app: ReviewBoardAppState) u8 {
    return review_board_journal_group(app.ui.mode, app.ui.focus) + review_board_journal_local(app.ui)
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
    if tag >= REVIEW_BOARD_JOURNAL_GROUP_FOCUS_BODY {
        return review_board_ui.ReviewBoardMode.Focus
    }
    return review_board_ui.ReviewBoardMode.Audit
}

func review_board_journal_focus_from_state(tag: u8) active_region.ActiveRegion {
    if tag >= REVIEW_BOARD_JOURNAL_GROUP_AUDIT_STATUS && tag < REVIEW_BOARD_JOURNAL_GROUP_FOCUS_BODY {
        return active_region.ActiveRegion.Status
    }
    if tag >= REVIEW_BOARD_JOURNAL_GROUP_FOCUS_STATUS {
        return active_region.ActiveRegion.Status
    }
    return active_region.ActiveRegion.Body
}

func review_board_journal_local_from_tag(tag: u8) u8 {
    if tag >= REVIEW_BOARD_JOURNAL_GROUP_FOCUS_STATUS {
        return tag - REVIEW_BOARD_JOURNAL_GROUP_FOCUS_STATUS
    }
    if tag >= REVIEW_BOARD_JOURNAL_GROUP_FOCUS_BODY {
        return tag - REVIEW_BOARD_JOURNAL_GROUP_FOCUS_BODY
    }
    if tag >= REVIEW_BOARD_JOURNAL_GROUP_AUDIT_STATUS {
        return tag - REVIEW_BOARD_JOURNAL_GROUP_AUDIT_STATUS
    }
    return tag
}

func review_board_journal_detail_from_local(local: u8) review_board_ui.ReviewBoardDetail {
    if local == REVIEW_BOARD_JOURNAL_LOCAL_OPEN || local == REVIEW_BOARD_JOURNAL_LOCAL_FILTER_PROMPT_OPEN {
        return review_board_ui.ReviewBoardDetail.Open
    }
    if local == REVIEW_BOARD_JOURNAL_LOCAL_CLOSED || local == REVIEW_BOARD_JOURNAL_LOCAL_FILTER_PROMPT_CLOSED {
        return review_board_ui.ReviewBoardDetail.Closed
    }
    if local == REVIEW_BOARD_JOURNAL_LOCAL_URGENT || local == REVIEW_BOARD_JOURNAL_LOCAL_FILTER_PROMPT_URGENT {
        return review_board_ui.ReviewBoardDetail.Urgent
    }
    if local < REVIEW_BOARD_JOURNAL_LOCAL_FILTER_FIRST_OPEN_AUDIT {
        return review_board_ui.ReviewBoardDetail.Summary
    }
    if local < REVIEW_BOARD_JOURNAL_LOCAL_FILTER_FIRST_CLOSED_AUDIT {
        return review_board_ui.ReviewBoardDetail.Open
    }
    if local < REVIEW_BOARD_JOURNAL_LOCAL_FILTER_FIRST_URGENT_AUDIT {
        return review_board_ui.ReviewBoardDetail.Closed
    }
    return review_board_ui.ReviewBoardDetail.Urgent
}

func review_board_journal_filtering_from_local(local: u8) bool {
    return local >= REVIEW_BOARD_JOURNAL_LOCAL_FILTER_PROMPT_SUMMARY
}

func review_board_journal_filterlen_from_local(local: u8) u8 {
    if local >= REVIEW_BOARD_JOURNAL_LOCAL_FILTER_FIRST_SUMMARY_AUDIT {
        return 1
    }
    if local >= REVIEW_BOARD_JOURNAL_LOCAL_FILTER_PROMPT_SUMMARY {
        return 0
    }
    return 0
}

func review_board_journal_filter0_from_local(local: u8) u8 {
    if local == REVIEW_BOARD_JOURNAL_LOCAL_FILTER_FIRST_SUMMARY_AUDIT || local == REVIEW_BOARD_JOURNAL_LOCAL_FILTER_FIRST_OPEN_AUDIT || local == REVIEW_BOARD_JOURNAL_LOCAL_FILTER_FIRST_CLOSED_AUDIT || local == REVIEW_BOARD_JOURNAL_LOCAL_FILTER_FIRST_URGENT_AUDIT {
        return review_board_ui.REVIEW_BOARD_KEY_AUDIT
    }
    if local == REVIEW_BOARD_JOURNAL_LOCAL_FILTER_FIRST_SUMMARY_CLOSED || local == REVIEW_BOARD_JOURNAL_LOCAL_FILTER_FIRST_OPEN_CLOSED || local == REVIEW_BOARD_JOURNAL_LOCAL_FILTER_FIRST_CLOSED_CLOSED || local == REVIEW_BOARD_JOURNAL_LOCAL_FILTER_FIRST_URGENT_CLOSED {
        return review_board_ui.REVIEW_BOARD_KEY_CLOSED
    }
    if local == REVIEW_BOARD_JOURNAL_LOCAL_FILTER_FIRST_SUMMARY_OPEN || local == REVIEW_BOARD_JOURNAL_LOCAL_FILTER_FIRST_OPEN_OPEN || local == REVIEW_BOARD_JOURNAL_LOCAL_FILTER_FIRST_CLOSED_OPEN || local == REVIEW_BOARD_JOURNAL_LOCAL_FILTER_FIRST_URGENT_OPEN {
        return review_board_ui.REVIEW_BOARD_KEY_OPEN
    }
    return review_board_ui.REVIEW_BOARD_KEY_URGENT
}

func review_board_journal_tag_valid(tag: u8) bool {
    return tag < REVIEW_BOARD_JOURNAL_TAG_LIMIT
}

func review_board_app_reload(journal: journal_service.JournalServiceState) ReviewBoardAppState {
    lane := journal_service.lane_at(journal, REVIEW_BOARD_JOURNAL_LANE)
    if lane.len != REVIEW_BOARD_JOURNAL_STATE_LEN {
        return review_board_app_init()
    }
    if !review_board_journal_tag_valid(lane.data[3]) {
        return review_board_app_init()
    }
    local := review_board_journal_local_from_tag(lane.data[3])
    return review_board_appwith(
        review_board_ui.review_board_uiwith_filter(
            lane.data[0],
            lane.data[1],
            lane.data[2],
            review_board_journal_mode_from_state(lane.data[3]),
            review_board_journal_focus_from_state(lane.data[3]),
            review_board_journal_detail_from_local(local),
            review_board_journal_filtering_from_local(local),
            review_board_journal_filter0_from_local(local),
            0,
            review_board_journal_filterlen_from_local(local)))
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