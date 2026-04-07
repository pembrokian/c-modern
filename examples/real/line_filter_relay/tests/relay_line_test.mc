import errors
import line_filter_relay
import mem
import testing

func test_relay_line() *i32 {
    output: *Buffer<u8>
    err: errors.Error
    output, err = line_filter_relay.relay_line("phase forty five", mem.default_allocator())
    test_err: *i32 = testing.expect_ok(err)
    if test_err != nil {
        return test_err
    }
    test_err = testing.expect_buffer_non_nil<u8>(output)
    if test_err != nil {
        return test_err
    }
    defer mem.buffer_free<u8>(output)

    bytes: Slice<u8> = mem.slice_from_buffer<u8>(output)
    actual: str = str{ ptr: bytes.ptr, len: bytes.len }
    return testing.expect_str_eq(actual, "PHASE FORTY FIVE\n")
}