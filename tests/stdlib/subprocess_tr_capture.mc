import errors
import io
import os
import strings

const SUBPROCESS_ERR_SHORT_WRITE: usize = 1
const SUBPROCESS_ERR_UNEXPECTED_EOF: usize = 2

func close_ignored(file: io.File) {
    ignored: errors.Error = io.close(file)
    if !errors.is_ok(ignored) {
        return
    }
}

func wait_ignored(child: *os.Child) {
    ignored_status: i32
    ignored_err: errors.Error
    ignored_status, ignored_err = os.wait(child)
    _ = ignored_status
    if !errors.is_ok(ignored_err) {
        return
    }
}

func write_exact(file: io.File, bytes: Slice<u8>) errors.Error {
    total: usize = 0
    while total < bytes.len {
        nwritten: usize
        err: errors.Error
        nwritten, err = io.write_file(file, bytes[total:bytes.len])
        if !errors.is_ok(err) {
            return err
        }
        if nwritten == 0 {
            return errors.fail_io(SUBPROCESS_ERR_SHORT_WRITE)
        }
        total = total + nwritten
    }
    return errors.ok()
}

func read_exact(file: io.File, bytes: Slice<u8>) errors.Error {
    total: usize = 0
    while total < bytes.len {
        nread: usize
        err: errors.Error
        nread, err = io.read(file, bytes[total:bytes.len])
        if !errors.is_ok(err) {
            return err
        }
        if nread == 0 {
            return errors.fail_io(SUBPROCESS_ERR_UNEXPECTED_EOF)
        }
        total = total + nread
    }
    return errors.ok()
}

func main(args: Slice<cstr>) i32 {
    _ = args

    input_pipe: io.Pipe
    err: errors.Error
    input_pipe, err = io.pipe()
    if !errors.is_ok(err) {
        return 10
    }
    defer close_ignored(input_pipe.read_end)
    defer close_ignored(input_pipe.write_end)

    output_pipe: io.Pipe
    output_pipe, err = io.pipe()
    if !errors.is_ok(err) {
        return 11
    }
    defer close_ignored(output_pipe.read_end)
    defer close_ignored(output_pipe.write_end)

    argv: [3]cstr
    argv[0] = "phase68-tr"
    argv[1] = "a-z"
    argv[2] = "A-Z"

    child: os.Child
    child, err = os.spawn_argv("/usr/bin/tr", (Slice<cstr>)(argv), input_pipe.read_end, output_pipe.write_end, input_pipe.write_end, output_pipe.read_end)
    if !errors.is_ok(err) {
        return 12
    }

    close_ignored(input_pipe.read_end)
    close_ignored(output_pipe.write_end)

    payload: Slice<u8> = strings.bytes("subprocess smoke")
    err = write_exact(input_pipe.write_end, payload)
    if !errors.is_ok(err) {
        wait_ignored(&child)
        return 13
    }
    err = write_exact(input_pipe.write_end, strings.bytes("\n"))
    if !errors.is_ok(err) {
        wait_ignored(&child)
        return 14
    }
    err = io.close(input_pipe.write_end)
    if !errors.is_ok(err) {
        wait_ignored(&child)
        return 15
    }

    output_storage: [17]u8
    output: Slice<u8> = (Slice<u8>)(output_storage)
    err = read_exact(output_pipe.read_end, output)
    if !errors.is_ok(err) {
        wait_ignored(&child)
        return 16
    }
    err = io.close(output_pipe.read_end)
    if !errors.is_ok(err) {
        wait_ignored(&child)
        return 17
    }

    status: i32
    status, err = os.wait(&child)
    if !errors.is_ok(err) {
        return 18
    }
    if status != 0 {
        return 19
    }

    if io.write(str{ ptr: output.ptr, len: output.len }) != 0 {
        return 20
    }
    return 0
}