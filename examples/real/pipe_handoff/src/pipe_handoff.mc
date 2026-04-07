import errors
import io
import strings

var writer_file: io.File = 0
var writer_bound: bool = false

func bind_runtime(file: io.File) {
    writer_file = file
    writer_bound = true
}

func expected_message() str {
    return "phase43-pipe"
}

func message_len() usize {
    return strings.byte_len(expected_message())
}

func message_matches(bytes: Slice<u8>) bool {
    expected: Slice<u8> = strings.bytes(expected_message())
    if bytes.len != expected.len {
        return false
    }

    index: usize = 0
    while index < expected.len {
        if bytes[index] != expected[index] {
            return false
        }
        index = index + 1
    }
    return true
}

func close_ignored(file: io.File) {
    ignored: errors.Error = io.close(file)
    if !errors.is_ok(ignored) {
        return
    }
}

func writer(status: *i32) {
    *status = 0
    if !writer_bound {
        *status = 1
        return
    }

    expected: Slice<u8> = strings.bytes(expected_message())
    nwritten: usize
    err: errors.Error
    nwritten, err = io.write_file(writer_file, expected)
    if !errors.is_ok(err) {
        *status = 2
        close_ignored(writer_file)
        writer_bound = false
        return
    }
    if nwritten != expected.len {
        *status = 3
        close_ignored(writer_file)
        writer_bound = false
        return
    }

    err = io.close(writer_file)
    if !errors.is_ok(err) {
        *status = 4
        return
    }
    writer_bound = false
}