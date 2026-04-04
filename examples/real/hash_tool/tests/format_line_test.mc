export { test_format_line }

import hash
import mem
import testing

func test_format_line() *i32 {
    line: *Buffer<u8> = hash.format_line(12289845435839482867, "sample.txt")
    err: *i32 = testing.expect(line != nil)
    if err != nil {
        return err
    }
    defer mem.buffer_free<u8>(line)

    bytes: Slice<u8> = mem.slice_from_buffer<u8>(line)
    actual: str = str{ ptr: bytes.ptr, len: bytes.len }
    err = testing.expect_str_eq(actual, "aa8e4d4f3bab0bf3  sample.txt")
    if err != nil {
        return err
    }
    return nil
}