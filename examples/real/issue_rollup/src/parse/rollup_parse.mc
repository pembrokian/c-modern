export { summarize_text }

import rollup_model
import strings

func line_kind(bytes: Slice<u8>, start: usize, newline: usize) u8 {
    if newline <= start {
        return 0
    }
    return bytes[start]
}

func line_is_priority(bytes: Slice<u8>, start: usize, newline: usize) bool {
    if newline <= start + 1 {
        return false
    }
    return bytes[start + 1] == 33
}

func summarize_text(text: str) rollup_model.Summary {
    bytes: Slice<u8> = strings.bytes(text)
    open_items: usize = 0
    closed_items: usize = 0
    blocked_items: usize = 0
    priority_items: usize = 0
    start: usize = 0
    while start < text.len {
        newline: usize = start
        while newline < text.len {
            if bytes[newline] == 10 {
                break
            }
            newline = newline + 1
        }

        kind: u8 = line_kind(bytes, start, newline)
        if kind == 79 {
            open_items = open_items + 1
        }
        if kind == 67 {
            closed_items = closed_items + 1
        }
        if kind == 66 {
            blocked_items = blocked_items + 1
        }
        if line_is_priority(bytes, start, newline) {
            priority_items = priority_items + 1
        }

        if newline == text.len {
            break
        }
        start = newline + 1
    }

    return rollup_model.Summary{ open_items: open_items, closed_items: closed_items, blocked_items: blocked_items, priority_items: priority_items }
}