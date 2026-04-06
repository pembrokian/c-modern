export { rollup_kind, write_rollup }

import io
import rollup_model

const ROLLUP_EMPTY: i32 = 0
const ROLLUP_STEADY: i32 = 1
const ROLLUP_BUSY: i32 = 2
const ROLLUP_ATTENTION: i32 = 3

func rollup_kind(summary: rollup_model.Summary) i32 {
    if rollup_model.total_items(summary) == 0 {
        return ROLLUP_EMPTY
    }
    if rollup_model.has_priority(summary) {
        return ROLLUP_ATTENTION
    }
    if summary.open_items > summary.closed_items {
        return ROLLUP_BUSY
    }
    return ROLLUP_STEADY
}

func write_rollup(summary: rollup_model.Summary) i32 {
    kind: i32 = rollup_kind(summary)
    if kind == ROLLUP_EMPTY {
        if io.write_line("issue-rollup-empty") != 0 {
            return 1
        }
        return 0
    }
    if kind == ROLLUP_ATTENTION {
        if io.write_line("issue-rollup-attention") != 0 {
            return 1
        }
        return 0
    }
    if kind == ROLLUP_BUSY {
        if io.write_line("issue-rollup-busy") != 0 {
            return 1
        }
        return 0
    }
    if io.write_line("issue-rollup-steady") != 0 {
        return 1
    }
    return 0
}