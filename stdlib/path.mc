import mem
import strings

@private
const PATH_SEPARATOR: u8 = 47

@private
func is_separator(value: u8) bool {
    return value == PATH_SEPARATOR
}

@private
func copy_bytes(dst: Slice<u8>, src: Slice<u8>) {
    index: usize = 0
    while index < src.len {
        dst[index] = src[index]
        index = index + 1
    }
}

@private
func copy_text(alloc: *mem.Allocator, text: str) *Buffer<u8> {
    buf: *Buffer<u8> = mem.buffer_new<u8>(alloc, text.len)
    if buf == nil {
        return nil
    }

    dst: Slice<u8> = mem.slice_from_buffer<u8>(buf)
    src: Slice<u8> = strings.bytes(text)
    copy_bytes(dst, src)
    return buf
}

@private
func trim_trailing_separators(text: str, limit: usize) usize {
    bytes: Slice<u8> = strings.bytes(text)
    end: usize = limit
    while end > 1 {
        if !is_separator(bytes[end - 1]) {
            break
        }
        end = end - 1
    }
    return end
}

@private
func last_separator_before(text: str, limit: usize) usize {
    bytes: Slice<u8> = strings.bytes(text)
    index: usize = limit
    while index > 0 {
        if is_separator(bytes[index - 1]) {
            return index - 1
        }
        index = index - 1
    }
    return text.len
}

func join(alloc: *mem.Allocator, base: str, name: str) *Buffer<u8> {
    if base.len == 0 {
        return copy_text(alloc, name)
    }
    if name.len == 0 {
        return copy_text(alloc, base)
    }

    name_bytes: Slice<u8> = strings.bytes(name)
    if is_separator(name_bytes[0]) {
        return copy_text(alloc, name)
    }

    base_bytes: Slice<u8> = strings.bytes(base)
    need_separator: bool = !is_separator(base_bytes[base.len - 1])
    extra: usize = 0
    if need_separator {
        extra = 1
    }

    total: usize = base.len + extra + name.len
    buf: *Buffer<u8> = mem.buffer_new<u8>(alloc, total)
    if buf == nil {
        return nil
    }

    dst: Slice<u8> = mem.slice_from_buffer<u8>(buf)
    cursor: usize = 0
    copy_bytes(dst[cursor:cursor + base.len], base_bytes)
    cursor = cursor + base.len
    if need_separator {
        dst[cursor] = PATH_SEPARATOR
        cursor = cursor + 1
    }
    copy_bytes(dst[cursor:cursor + name.len], name_bytes)
    return buf
}

func basename(text: str) str {
    if text.len == 0 {
        return "."
    }

    bytes: Slice<u8> = strings.bytes(text)
    end: usize = trim_trailing_separators(text, text.len)
    if end == 1 {
        if is_separator(bytes[0]) {
            return "/"
        }
    }

    sep: usize = last_separator_before(text, end)
    if sep == text.len {
        return text[0:end]
    }
    return text[sep + 1:end]
}

func dirname(text: str) str {
    if text.len == 0 {
        return "."
    }

    bytes: Slice<u8> = strings.bytes(text)
    end: usize = trim_trailing_separators(text, text.len)
    if end == 1 {
        if is_separator(bytes[0]) {
            return "/"
        }
    }

    sep: usize = last_separator_before(text, end)
    if sep == text.len {
        return "."
    }

    parent_end: usize = trim_trailing_separators(text, sep)
    if sep == 0 {
        return "/"
    }
    return text[0:parent_end]
}
