export { Summary, has_priority, total_items }

struct Summary {
    open_items: usize
    closed_items: usize
    blocked_items: usize
    priority_items: usize
}

func total_items(summary: Summary) usize {
    return summary.open_items + summary.closed_items + summary.blocked_items
}

func has_priority(summary: Summary) bool {
    return summary.priority_items > 0
}