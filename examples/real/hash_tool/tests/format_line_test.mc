export { test_format_line }

import hash
import mem
import testing

func same_text(left: str, right: str) bool {
    if left.len != right.len {
        return false
    }

    left_bytes: Slice<u8> = Slice<u8>{ ptr: left.ptr, len: left.len }
    right_bytes: Slice<u8> = Slice<u8>{ ptr: right.ptr, len: right.len }
    index: usize = 0
    while index < left.len {
        if left_bytes[index] != right_bytes[index] {
            return false
        }
        index = index + 1
    }
    return true
}

func test_format_line() *i32 {
    line: *Buffer<u8> = hash.format_line(12289845435839482867, "sample.txt")
    if line == nil {
        return testing.fail()
    }
    defer mem.buffer_free<u8>(line)

    bytes: Slice<u8> = mem.slice_from_buffer<u8>(line)
    actual: str = str{ ptr: bytes.ptr, len: bytes.len }
    if !same_text(actual, "aa8e4d4f3bab0bf3  sample.txt") {
        return testing.fail()
    }
    return nil
}