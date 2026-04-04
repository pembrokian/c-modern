export { test_hex_u64 }

import hash
import mem
import strings
import testing

func same_text(left: str, right: str) bool {
    if left.len != right.len {
        return false
    }

    left_bytes: Slice<u8> = strings.bytes(left)
    right_bytes: Slice<u8> = strings.bytes(right)
    index: usize = 0
    while index < left.len {
        if left_bytes[index] != right_bytes[index] {
            return false
        }
        index = index + 1
    }
    return true
}

func test_hex_u64() *i32 {
    text: *Buffer<u8> = hash.hex_u64(12289845435839482867)
    if text == nil {
        return testing.fail()
    }
    defer mem.buffer_free<u8>(text)

    bytes: Slice<u8> = mem.slice_from_buffer<u8>(text)
    actual: str = str{ ptr: bytes.ptr, len: bytes.len }
    if !same_text(actual, "aa8e4d4f3bab0bf3") {
        return testing.fail()
    }
    return nil
}