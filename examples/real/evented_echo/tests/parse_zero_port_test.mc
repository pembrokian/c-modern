import errors
import net
import testing

func test_parse_zero_port() *i32 {
    value: u16
    err: errors.Error
    value, err = net.parse_port("0")
    if errors.is_err(err) {
        return testing.fail()
    }
    if value != 0 {
        return testing.fail()
    }
    return nil
}