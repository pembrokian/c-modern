import errors
import line_filter_relay
import testing

func test_relay_child_split_diagnostics() *i32 {
    stdout_used: usize
    stderr_used: usize
    status: i32
    err: errors.Error
    stdout_used, stderr_used, status, err = line_filter_relay.capture_invalid_tr_split_diagnostics()
    test_err: *i32 = testing.expect_ok(err)
    if test_err != nil {
        return test_err
    }
    test_err = testing.expect_usize_eq(stdout_used, 0)
    if test_err != nil {
        return test_err
    }
    test_err = testing.expect_false(stderr_used == 0)
    if test_err != nil {
        return test_err
    }
    return testing.expect_false(status == 0)
}