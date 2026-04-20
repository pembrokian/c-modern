enum ReviewBoardMode {
    Audit,
    Focus,
}

enum ReviewBoardFocus {
    Body,
    Status,
}

const REVIEW_BOARD_KEY_AUDIT: u8 = 65
const REVIEW_BOARD_KEY_CLOSED: u8 = 67
const REVIEW_BOARD_KEY_FOCUS: u8 = 70
const REVIEW_BOARD_KEY_OPEN: u8 = 79
const REVIEW_BOARD_KEY_RESET: u8 = 82
const REVIEW_BOARD_KEY_TOGGLE_REGION: u8 = 84
const REVIEW_BOARD_KEY_URGENT: u8 = 85

struct ReviewBoardUiState {
    open: u8
    closed: u8
    urgent: u8
    mode: ReviewBoardMode
    focus: ReviewBoardFocus
}

struct ReviewBoardUiResult {
    state: ReviewBoardUiState
    changed: bool
}

func review_board_ui_init() ReviewBoardUiState {
    return ReviewBoardUiState{
        open: 0,
        closed: 0,
        urgent: 0,
        mode: ReviewBoardMode.Audit,
        focus: ReviewBoardFocus.Body }
}

func review_board_uiwith(open: u8, closed: u8, urgent: u8, mode: ReviewBoardMode, focus: ReviewBoardFocus) ReviewBoardUiState {
    return ReviewBoardUiState{ open: open, closed: closed, urgent: urgent, mode: mode, focus: focus }
}

func review_board_ui_result(state: ReviewBoardUiState, changed: bool) ReviewBoardUiResult {
    return ReviewBoardUiResult{ state: state, changed: changed }
}

func review_board_ui_toggle_focus(focus: ReviewBoardFocus) ReviewBoardFocus {
    switch focus {
    case ReviewBoardFocus.Status:
        return ReviewBoardFocus.Body
    default:
        return ReviewBoardFocus.Status
    }
}

func review_board_ui_audit_should_pause(state: ReviewBoardUiState) bool {
    return state.open > state.closed
}

func review_board_ui_focus_needs_escalation(state: ReviewBoardUiState) bool {
    return state.urgent > 2
}

func review_board_ui_result_cells(state: ReviewBoardUiState) [2]u8 {
    switch state.mode {
    case ReviewBoardMode.Focus:
        if review_board_ui_focus_needs_escalation(state) {
            return [2]u8{ 70, 69 }
        }
        return [2]u8{ 70, 78 }
    default:
        if review_board_ui_audit_should_pause(state) {
            return [2]u8{ 65, 80 }
        }
        return [2]u8{ 65, 83 }
    }
}

func review_board_ui_status_cells(state: ReviewBoardUiState) [2]u8 {
    mode_cell: u8 = 65
    if state.mode == ReviewBoardMode.Focus {
        mode_cell = 70
    }

    focus_cell: u8 = 66
    if state.focus == ReviewBoardFocus.Status {
        focus_cell = 83
    }
    return [2]u8{ mode_cell, focus_cell }
}

func review_board_ui_display_cells(state: ReviewBoardUiState) [4]u8 {
    result := review_board_ui_result_cells(state)
    status := review_board_ui_status_cells(state)
    return [4]u8{ result[0], result[1], status[0], status[1] }
}

func review_board_ui_body_key(state: ReviewBoardUiState, key: u8) ReviewBoardUiResult {
    switch key {
    case REVIEW_BOARD_KEY_OPEN:
        return review_board_ui_result(review_board_uiwith(state.open + 1, state.closed, state.urgent, state.mode, state.focus), true)
    case REVIEW_BOARD_KEY_CLOSED:
        return review_board_ui_result(review_board_uiwith(state.open, state.closed + 1, state.urgent, state.mode, state.focus), true)
    case REVIEW_BOARD_KEY_URGENT:
        return review_board_ui_result(review_board_uiwith(state.open + 1, state.closed, state.urgent + 1, state.mode, state.focus), true)
    case REVIEW_BOARD_KEY_RESET:
        if state.open == 0 && state.closed == 0 && state.urgent == 0 {
            return review_board_ui_result(state, false)
        }
        return review_board_ui_result(review_board_uiwith(0, 0, 0, state.mode, state.focus), true)
    default:
        return review_board_ui_result(state, false)
    }
}

func review_board_ui_status_key(state: ReviewBoardUiState, key: u8) ReviewBoardUiResult {
    switch key {
    case REVIEW_BOARD_KEY_AUDIT:
        if state.mode == ReviewBoardMode.Audit {
            return review_board_ui_result(state, false)
        }
        return review_board_ui_result(review_board_uiwith(state.open, state.closed, state.urgent, ReviewBoardMode.Audit, state.focus), true)
    case REVIEW_BOARD_KEY_FOCUS:
        if state.mode == ReviewBoardMode.Focus {
            return review_board_ui_result(state, false)
        }
        return review_board_ui_result(review_board_uiwith(state.open, state.closed, state.urgent, ReviewBoardMode.Focus, state.focus), true)
    default:
        return review_board_ui_result(state, false)
    }
}

func review_board_ui_handle_key(state: ReviewBoardUiState, key: u8) ReviewBoardUiResult {
    if key == REVIEW_BOARD_KEY_TOGGLE_REGION {
        return review_board_ui_result(
            review_board_uiwith(state.open, state.closed, state.urgent, state.mode, review_board_ui_toggle_focus(state.focus)),
            true)
    }

    switch state.focus {
    case ReviewBoardFocus.Status:
        return review_board_ui_status_key(state, key)
    default:
        return review_board_ui_body_key(state, key)
    }
}