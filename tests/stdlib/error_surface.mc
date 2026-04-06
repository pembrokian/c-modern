import errors
import io
import net
import utf8

const TEST_ERR_USER: usize = 11
const TEST_ERR_ERRNO: usize = 22
const TEST_ERR_NET: usize = 33
const TEST_INVALID_PORT_CODE: usize = 1

func main() i32 {
    err: errors.Error = errors.fail_io(7)
    if errors.is_ok(err) {
        return 1
    }
    if errors.kind(err) != errors.kind_io() {
        return 2
    }
    if errors.code(err) != 7 {
        return 3
    }
    if !errors.is_err(err) {
        return 10
    }

    decoded: utf8.Decoded
    decoded, err = utf8.decode("")
    _ = decoded
    if errors.is_ok(err) {
        return 4
    }
    if errors.kind(err) != errors.kind_utf8() {
        return 5
    }
    if errors.code(err) != 1 {
        return 6
    }

    err = io.close(-1)
    if errors.is_ok(err) {
        return 7
    }
    if errors.kind(err) != errors.kind_errno() {
        return 8
    }
    if errors.code(err) == 0 {
        return 9
    }

    err = errors.fail_user(TEST_ERR_USER)
    if errors.kind(err) != errors.kind_user() {
        return 11
    }
    if errors.code(err) != TEST_ERR_USER {
        return 12
    }

    err = errors.fail_errno(TEST_ERR_ERRNO)
    if errors.kind(err) != errors.kind_errno() {
        return 13
    }
    if errors.code(err) != TEST_ERR_ERRNO {
        return 14
    }

    err = errors.fail_net(TEST_ERR_NET)
    if errors.kind(err) != errors.kind_net() {
        return 15
    }
    if errors.code(err) != TEST_ERR_NET {
        return 16
    }

    invalid_port: u16
    invalid_port, err = net.parse_port("70000")
    if errors.is_ok(err) {
        return 17
    }
    if invalid_port != 0 {
        return 18
    }
    if errors.kind(err) != errors.kind_net() {
        return 19
    }
    if errors.code(err) != TEST_INVALID_PORT_CODE {
        return 20
    }

    return 0
}