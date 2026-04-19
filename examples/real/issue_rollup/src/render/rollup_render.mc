import io
import rollup_model

const ROLLUP_EMPTY: i32 = 0
const ROLLUP_STEADY: i32 = 1
const ROLLUP_BUSY: i32 = 2
const ROLLUP_ATTENTION: i32 = 3

@private
func write_empty() i32 {
    if io.write_line("issue-rollup-empty") != 0 {
        return 1
    }
    return 0
}

@private
func write_steady() i32 {
    if io.write_line("issue-rollup-steady") != 0 {
        return 1
    }
    return 0
}

@private
func write_busy() i32 {
    if io.write_line("issue-rollup-busy") != 0 {
        return 1
    }
    return 0
}

@private
func write_attention() i32 {
    if io.write_line("issue-rollup-attention") != 0 {
        return 1
    }
    return 0
}

@private
const ROLLUP_WRITERS: [4]func() i32 = [4]func() i32{
    write_empty,
    write_steady,
    write_busy,
    write_attention,
}

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
    writer: func() i32 = ROLLUP_WRITERS[usize(kind)]
    return writer()
}