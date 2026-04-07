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

    stdout_pipe: io.Pipe
    stdout_pipe, err = io.pipe()
    if !errors.is_ok(err) {
        return 11
    }
    defer close_ignored(stdout_pipe.read_end)
    defer close_ignored(stdout_pipe.write_end)

    stderr_pipe: io.Pipe
    stderr_pipe, err = io.pipe()
    if !errors.is_ok(err) {
        return 12
    }
    defer close_ignored(stderr_pipe.read_end)
    defer close_ignored(stderr_pipe.write_end)

    argv: [2]cstr
    argv[0] = "tr"
    argv[1] = "--definitely-invalid-option"

    child: os.Child
    child, err = os.spawn_pipe_argv_split_output("/usr/bin/tr", (Slice<cstr>)(argv), input_pipe, stdout_pipe, stderr_pipe)
    if !errors.is_ok(err) {
        return 13
    }

    close_ignored(input_pipe.read_end)
    close_ignored(stdout_pipe.write_end)
    close_ignored(stderr_pipe.write_end)

    err = io.close(input_pipe.write_end)
    if !errors.is_ok(err) {
        wait_ignored(&child)
        return 14
    }

    stdout_storage: [32]u8
    stdout_output: Slice<u8> = (Slice<u8>)(stdout_storage)
    stdout_used: usize
    stdout_used, err = read_until_eof(stdout_pipe.read_end, stdout_output)
    if !errors.is_ok(err) {
        wait_ignored(&child)
        return 15
    }
    err = io.close(stdout_pipe.read_end)
    if !errors.is_ok(err) {
        wait_ignored(&child)
        return 16
    }

    stderr_storage: [512]u8
    stderr_output: Slice<u8> = (Slice<u8>)(stderr_storage)
    stderr_used: usize
    stderr_used, err = read_until_eof(stderr_pipe.read_end, stderr_output)
    if !errors.is_ok(err) {
        wait_ignored(&child)
        return 17
    }
    err = io.close(stderr_pipe.read_end)
    if !errors.is_ok(err) {
        wait_ignored(&child)
        return 18
    }

    status: i32
    status, err = os.wait(&child)
    if !errors.is_ok(err) {
        return 19
    }
    if stdout_used != 0 {
        return 20
    }
    if stderr_used == 0 {
        return 21
    }
    if status == 0 {
        return 22
    }
    if io.write_line("SUBPROCESS STDERR SPLIT SMOKE") != 0 {
        return 23
    }
    return 0
}