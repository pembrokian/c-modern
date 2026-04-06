export { test_parse_invalid_port }

import errors
import net
import testing

func test_parse_invalid_port() *i32 {
    value: u16
    err: errors.Error
    value, err = net.parse_port("70000")
    if errors.is_ok(err) {
        return testing.fail()
    }
    if value != 0 {
        return testing.fail()
    }

    value, err = net.parse_port("40x0")
    if errors.is_ok(err) {
        return testing.fail()
    }
    if value != 0 {
        return testing.fail()
    }
    return nil
}