import errors
import net
import testing

func test_parse_valid_port() *i32 {
    value: u16
    err: errors.Error
    value, err = net.parse_port("4040")
    if errors.is_err(err) {
        return testing.fail()
    }
    if value != 4040 {
        return testing.fail()
    }
    return nil
}