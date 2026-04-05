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
        return io.write_line("issue-rollup-empty")
    }
    if kind == ROLLUP_ATTENTION {
        return io.write_line("issue-rollup-attention")
    }
    if kind == ROLLUP_BUSY {
        return io.write_line("issue-rollup-busy")
    }
    return io.write_line("issue-rollup-steady")
}