export { test_parse_zero_port }

import echo_core
import testing

func test_parse_zero_port() *i32 {
    value: u16
    err: uintptr
    value, err = echo_core.parse_port("0")
    if err != 0 {
        return testing.fail()
    }
    if value != 0 {
        return testing.fail()
    }
    return nil
}