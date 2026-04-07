import errors
import io
import os

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

func read_until_eof(file: io.File, bytes: Slice<u8>) (usize, errors.Error) {
    total: usize = 0
    while total < bytes.len {
        nread: usize
        err: errors.Error
        nread, err = io.read(file, bytes[total:bytes.len])
        if !errors.is_ok(err) {
            return 0, err
        }
        if nread == 0 {
            return total, errors.ok()
        }
        total = total + nread
    }
    return total, errors.ok()
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

    argv: [2]cstr
    argv[0] = "tr"
    argv[1] = "--definitely-invalid-option"

    child: os.Child
    child, err = os.spawn_pipe_argv_merged_output("/usr/bin/tr", (Slice<cstr>)(argv), input_pipe, output_pipe)
    if !errors.is_ok(err) {
        return 12
    }

    close_ignored(input_pipe.read_end)
    close_ignored(output_pipe.write_end)

    err = io.close(input_pipe.write_end)
    if !errors.is_ok(err) {
        wait_ignored(&child)
        return 13
    }

    output_storage: [256]u8
    output: Slice<u8> = (Slice<u8>)(output_storage)
    nread: usize
    nread, err = read_until_eof(output_pipe.read_end, output)
    if !errors.is_ok(err) {
        wait_ignored(&child)
        return 14
    }
    err = io.close(output_pipe.read_end)
    if !errors.is_ok(err) {
        wait_ignored(&child)
        return 15
    }

    status: i32
    status, err = os.wait(&child)
    if !errors.is_ok(err) {
        return 16
    }
    if nread == 0 {
        return 17
    }
    if status == 0 {
        return 18
    }
    if io.write_line("SUBPROCESS STDERR SMOKE") != 0 {
        return 19
    }
    return 0
}