import rollup_model
import rollup_parse

const ISSUE_ROLLUP_TEXT_CAPACITY: usize = 96

const ISSUE_ROLLUP_KEY_OPEN: u8 = 79
const ISSUE_ROLLUP_KEY_PRIORITY: u8 = 80
const ISSUE_ROLLUP_KEY_RESET: u8 = 82

const ISSUE_ROLLUP_OPEN_LINE: [7]u8 = { 79, 32, 111, 112, 101, 110, 10 }
const ISSUE_ROLLUP_PRIORITY_LINE: [10]u8 = { 79, 33, 32, 117, 114, 103, 101, 110, 116, 10 }

struct IssueRollupInteractiveState {
    text: [ISSUE_ROLLUP_TEXT_CAPACITY]u8
    len: usize
}

struct IssueRollupInteractiveResult {
    state: IssueRollupInteractiveState
    summary: rollup_model.Summary
    changed: bool
}

func issue_rollup_interactive_init() IssueRollupInteractiveState {
    text: [ISSUE_ROLLUP_TEXT_CAPACITY]u8
    for i in 0..ISSUE_ROLLUP_TEXT_CAPACITY {
        text[i] = 0
    }
    return IssueRollupInteractiveState{ text: text, len: 0 }
}

func issue_rollup_interactivewith(text: [ISSUE_ROLLUP_TEXT_CAPACITY]u8, len: usize) IssueRollupInteractiveState {
    return IssueRollupInteractiveState{ text: text, len: len }
}

func issue_rollup_interactive_result(state: IssueRollupInteractiveState, summary: rollup_model.Summary, changed: bool) IssueRollupInteractiveResult {
    return IssueRollupInteractiveResult{ state: state, summary: summary, changed: changed }
}

func issue_rollup_summary(state: IssueRollupInteractiveState) rollup_model.Summary {
    return rollup_parse.summarize_text(str{ ptr: &state.text[0], len: state.len })
}

func issue_rollup_open_line() Slice<u8> {
    return Slice<u8>{ ptr: &ISSUE_ROLLUP_OPEN_LINE[0], len: 7 }
}

func issue_rollup_priority_line() Slice<u8> {
    return Slice<u8>{ ptr: &ISSUE_ROLLUP_PRIORITY_LINE[0], len: 10 }
}

func issue_rollup_append_line(state: IssueRollupInteractiveState, line: Slice<u8>) IssueRollupInteractiveResult {
    if state.len + line.len > ISSUE_ROLLUP_TEXT_CAPACITY {
        return issue_rollup_interactive_result(state, issue_rollup_summary(state), false)
    }

    next_text := state.text
    for i in 0..line.len {
        next_text[state.len + i] = line[i]
    }

    next_state := issue_rollup_interactivewith(next_text, state.len + line.len)
    return issue_rollup_interactive_result(next_state, issue_rollup_summary(next_state), true)
}

func issue_rollup_handle_key(state: IssueRollupInteractiveState, key: u8) IssueRollupInteractiveResult {
    switch key {
    case ISSUE_ROLLUP_KEY_OPEN:
        return issue_rollup_append_line(state, issue_rollup_open_line())
    case ISSUE_ROLLUP_KEY_PRIORITY:
        return issue_rollup_append_line(state, issue_rollup_priority_line())
    case ISSUE_ROLLUP_KEY_RESET:
        if state.len == 0 {
            return issue_rollup_interactive_result(state, issue_rollup_summary(state), false)
        }
        next_state := issue_rollup_interactive_init()
        return issue_rollup_interactive_result(next_state, issue_rollup_summary(next_state), true)
    default:
        return issue_rollup_interactive_result(state, issue_rollup_summary(state), false)
    }
}