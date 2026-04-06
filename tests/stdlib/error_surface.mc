import errors
import io
import utf8

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

    err = errors.fail_user(11)
    if errors.kind(err) != errors.kind_user() {
        return 11
    }
    if errors.code(err) != 11 {
        return 12
    }

    err = errors.fail_errno(22)
    if errors.kind(err) != errors.kind_errno() {
        return 13
    }
    if errors.code(err) != 22 {
        return 14
    }

    return 0
}