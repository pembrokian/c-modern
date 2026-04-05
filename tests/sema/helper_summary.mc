export { DEFAULT_SUMMARY, Summary }

struct Summary {
    open_items: usize
    closed_items: usize
    blocked_items: usize
    priority_items: usize
}

const DEFAULT_SUMMARY: Summary = Summary{ open_items: 1, closed_items: 1, blocked_items: 0, priority_items: 0 }