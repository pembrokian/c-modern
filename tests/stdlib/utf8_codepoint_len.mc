import mem
import utf8

func main() i32 {
    if utf8.leading_codepoint_width("") != 0 {
        return 1
    }
    if utf8.leading_codepoint_width("ascii") != 1 {
        return 2
    }
    if utf8.leading_codepoint_width("é") != 2 {
        return 3
    }
    if utf8.leading_codepoint_width("π") != 2 {
        return 4
    }

    alloc: *mem.Allocator = mem.default_allocator()
    bad = mem.buffer_new<u8>(alloc, 1)
    bytes = mem.slice_from_buffer<u8>(bad)
    bytes[0] = 128
    text = str{ ptr: bytes.ptr, len: bytes.len }
    if utf8.leading_codepoint_width(text) != 0 {
        mem.buffer_free<u8>(bad)
        return 5
    }
    mem.buffer_free<u8>(bad)
    return 0
}