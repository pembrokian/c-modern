export { audit_should_pause_text, focus_needs_escalation_text, run_audit, run_focus }

import fs
import io
import mem
import review_scan

func buffer_text(buf: *Buffer<u8>) str {
    bytes: Slice<u8> = mem.slice_from_buffer<u8>(buf)
    return str{ ptr: bytes.ptr, len: bytes.len }
}

func load_review_text(path: str) *Buffer<u8> {
    return fs.read_all(path, mem.default_allocator())
}

func audit_should_pause_text(text: str) bool {
    return review_scan.count_open_items(text) > review_scan.count_closed_items(text)
}

func focus_needs_escalation_text(text: str) bool {
    return review_scan.count_urgent_open_items(text) > 2
}

func run_audit(path: str) i32 {
    text_buf: *Buffer<u8> = load_review_text(path)
    if text_buf == nil {
        return 91
    }
    defer mem.buffer_free<u8>(text_buf)

    if audit_should_pause_text(buffer_text(text_buf)) {
        if io.write_line("review-audit-pause") != 0 {
            return 92
        }
        return 0
    }

    if io.write_line("review-audit-stable") != 0 {
        return 93
    }
    return 0
}

func run_focus(path: str) i32 {
    text_buf: *Buffer<u8> = load_review_text(path)
    if text_buf == nil {
        return 94
    }
    defer mem.buffer_free<u8>(text_buf)

    if focus_needs_escalation_text(buffer_text(text_buf)) {
        if io.write_line("review-focus-escalate") != 0 {
            return 95
        }
        return 0
    }

    if io.write_line("review-focus-normal") != 0 {
        return 96
    }
    return 0
}