import errors
import line_filter_relay
import mem
import testing

func test_relay_child_diagnostics() *i32 {
    output: *Buffer<u8>
    used: usize
    status: i32
    err: errors.Error
    output, used, status, err = line_filter_relay.capture_invalid_tr_diagnostics(mem.default_allocator())
    test_err: *i32 = testing.expect_ok(err)
    if test_err != nil {
        return test_err
    }
    test_err = testing.expect_buffer_non_nil<u8>(output)
    if test_err != nil {
        return test_err
    }
    defer mem.buffer_free<u8>(output)

    test_err = testing.expect_false(used == 0)
    if test_err != nil {
        return test_err
    }
    return testing.expect_false(status == 0)
}