export { test_parse_valid_port }

import echo_core
import testing

func test_parse_valid_port() *i32 {
    value: u16
    err: uintptr
    value, err = echo_core.parse_port("4040")
    if err != 0 {
        return testing.fail()
    }
    if value != 4040 {
        return testing.fail()
    }
    return nil
}