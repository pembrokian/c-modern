import errors
import net
import testing

const EXPECTED_INVALID_PORT_CODE: usize = 1

func test_parse_invalid_port() *i32 {
    value: u16
    err: errors.Error
    value, err = net.parse_port("70000")
    if errors.is_ok(err) {
        return testing.fail()
    }
    if errors.kind(err) != errors.kind_net() {
        return testing.fail()
    }
    if errors.code(err) != EXPECTED_INVALID_PORT_CODE {
        return testing.fail()
    }
    if value != 0 {
        return testing.fail()
    }

    value, err = net.parse_port("40x0")
    if errors.is_ok(err) {
        return testing.fail()
    }
    if errors.kind(err) != errors.kind_net() {
        return testing.fail()
    }
    if errors.code(err) != EXPECTED_INVALID_PORT_CODE {
        return testing.fail()
    }
    if value != 0 {
        return testing.fail()
    }
    return nil
}