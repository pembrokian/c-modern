import errors
import net
import testing

func test_parse_valid_port() *i32 {
    value: u16
    err: errors.Error
    value, err = net.parse_port("4040")
    test_err: *i32 = testing.expect_ok(err)
    if test_err != nil {
        return test_err
    }
    return testing.expect_usize_eq(usize(value), 4040)
}