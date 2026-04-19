import io
import rollup_model
import rollup_render_table

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
    writer: func() i32 = rollup_render_table.ROLLUP_WRITERS[usize(kind)]
    return writer()
}