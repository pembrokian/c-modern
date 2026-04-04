export { test_hex_u64 }

import hash
import mem
import testing

func test_hex_u64() *i32 {
    text: *Buffer<u8> = hash.hex_u64(12289845435839482867)
    err: *i32 = testing.expect(text != nil)
    if err != nil {
        return err
    }
    defer mem.buffer_free<u8>(text)

    bytes: Slice<u8> = mem.slice_from_buffer<u8>(text)
    actual: str = str{ ptr: bytes.ptr, len: bytes.len }
    err = testing.expect_str_eq(actual, "aa8e4d4f3bab0bf3")
    if err != nil {
        return err
    }
    return nil
}