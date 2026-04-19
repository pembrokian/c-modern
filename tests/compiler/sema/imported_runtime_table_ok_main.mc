import rollup_render_table_helper

struct Summary {
    open_items: usize
    closed_items: usize
    blocked_items: usize
    priority_items: usize
}

const ROLLUP_EMPTY: i32 = 0
const ROLLUP_STEADY: i32 = 1
const ROLLUP_BUSY: i32 = 2
const ROLLUP_ATTENTION: i32 = 3

const LOCAL_WRITERS: [4]func() i32 = rollup_render_table_helper.ROLLUP_WRITERS

func total_items(summary: Summary) usize {
    return summary.open_items + summary.closed_items + summary.blocked_items
}

func has_priority(summary: Summary) bool {
    return summary.priority_items > 0
}

func rollup_kind(summary: Summary) i32 {
    if total_items(summary) == 0 {
        return ROLLUP_EMPTY
    }
    if has_priority(summary) {
        return ROLLUP_ATTENTION
    }
    if summary.open_items > summary.closed_items {
        return ROLLUP_BUSY
    }
    return ROLLUP_STEADY
}

func write_rollup(summary: Summary) i32 {
    kind: i32 = rollup_kind(summary)
    writer: func() i32 = LOCAL_WRITERS[usize(kind)]
    return writer()
}

func main() i32 {
    total: i32 = write_rollup(Summary{ open_items: 0, closed_items: 0, blocked_items: 0, priority_items: 0 })
    total = total + write_rollup(Summary{ open_items: 1, closed_items: 1, blocked_items: 0, priority_items: 1 })
    total = total + write_rollup(Summary{ open_items: 3, closed_items: 1, blocked_items: 0, priority_items: 0 })
    total = total + write_rollup(Summary{ open_items: 1, closed_items: 3, blocked_items: 0, priority_items: 0 })
    return total
}