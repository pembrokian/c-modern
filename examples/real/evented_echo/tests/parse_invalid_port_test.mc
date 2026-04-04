export { test_parse_invalid_port }

import echo_core
import testing

func test_parse_invalid_port() *i32 {
    value: u16
    err: uintptr
    value, err = echo_core.parse_port("70000")
    if err == 0 {
        return testing.fail()
    }
    if value != 0 {
        return testing.fail()
    }

    value, err = echo_core.parse_port("40x0")
    if err == 0 {
        return testing.fail()
    }
    if value != 0 {
        return testing.fail()
    }
    return nil
}