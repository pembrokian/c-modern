import errors
import net
import testing

const EXPECTED_INVALID_PORT_CODE: usize = 1

func test_parse_invalid_port() *i32 {
    value: u16
    err: errors.Error
    value, err = net.parse_port("70000")
    test_err: *i32 = testing.expect_err(err, errors.kind_net(), EXPECTED_INVALID_PORT_CODE)
    if test_err != nil {
        return test_err
    }
    test_err = testing.expect_usize_eq(usize(value), 0)
    if test_err != nil {
        return test_err
    }

    value, err = net.parse_port("40x0")
    test_err = testing.expect_err(err, errors.kind_net(), EXPECTED_INVALID_PORT_CODE)
    if test_err != nil {
        return test_err
    }
    test_err = testing.expect_usize_eq(usize(value), 0)
    if test_err != nil {
        return test_err
    }
    return nil
}