import fs
import io
import mem
import strings

func contains(text: str, needle: str) bool {
    if needle.len == 0 {
        return true
    }
    if text.len < needle.len {
        return false
    }

    text_bytes: Slice<u8> = strings.bytes(text)
    needle_bytes: Slice<u8> = strings.bytes(needle)
    start: usize = 0
    limit: usize = text.len - needle.len
    while start <= limit {
        matched: bool = true
        index: usize = 0
        while index < needle.len {
            if text_bytes[start + index] != needle_bytes[index] {
                matched = false
                break
            }
            index = index + 1
        }
        if matched {
            return true
        }
        start = start + 1
    }
    return false
}

func count_matches(text: str, needle: str) usize {
    if text.len == 0 {
        return 0
    }

    bytes: Slice<u8> = strings.bytes(text)
    count: usize = 0
    start: usize = 0
    while start < text.len {
        newline: usize = start
        while newline < text.len {
            if bytes[newline] == 10 {
                break
            }
            newline = newline + 1
        }

        line: str = text[start:newline]
        if contains(line, needle) {
            count = count + 1
        }

        if newline == text.len {
            break
        }
        start = newline + 1
    }
    return count
}

func print_matches(text: str, needle: str) usize {
    if text.len == 0 {
        return 0
    }

    bytes: Slice<u8> = strings.bytes(text)
    printed: usize = 0
    start: usize = 0
    while start < text.len {
        newline: usize = start
        while newline < text.len {
            if bytes[newline] == 10 {
                break
            }
            newline = newline + 1
        }

        line: str = text[start:newline]
        if contains(line, needle) {
            if io.write_line(line) != 0 {
                return printed
            }
            printed = printed + 1
        }

        if newline == text.len {
            break
        }
        start = newline + 1
    }
    return printed
}

func run(needle: str, path: str) i32 {
    alloc: *mem.Allocator = mem.default_allocator()
    buf: *Buffer<u8> = fs.read_all(path, alloc)
    if buf == nil {
        return 92
    }
    defer mem.buffer_free<u8>(buf)

    bytes: Slice<u8> = mem.slice_from_buffer<u8>(buf)
    text: str = str{ ptr: bytes.ptr, len: bytes.len }
    matches: usize = print_matches(text, needle)
    if matches == 0 {
        return 1
    }
    return 0
}
