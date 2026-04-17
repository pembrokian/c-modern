import errors
import io
import mem
import os
import strings

const RELAY_ERR_SHORT_WRITE: usize = 1
const RELAY_ERR_UNEXPECTED_EOF: usize = 2
const RELAY_ERR_OUTPUT_ALLOC: usize = 3
const RELAY_ERR_CHILD_EXIT: usize = 4
const RELAY_ERR_DIAGNOSTIC_ALLOC: usize = 5

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
    if ignored_status != 0 {
    }
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
            return errors.fail_io(RELAY_ERR_SHORT_WRITE)
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
            return errors.fail_io(RELAY_ERR_UNEXPECTED_EOF)
        }
        total = total + nread
    }
    return errors.ok()
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

func relay_line(line: str, alloc: *mem.Allocator) (*Buffer<u8>, errors.Error) {
    input_pipe: io.Pipe
    err: errors.Error
    input_pipe, err = io.pipe()
    if !errors.is_ok(err) {
        return nil, err
    }
    defer close_ignored(input_pipe.read_end)
    defer close_ignored(input_pipe.write_end)

    output_pipe: io.Pipe
    output_pipe, err = io.pipe()
    if !errors.is_ok(err) {
        return nil, err
    }
    defer close_ignored(output_pipe.read_end)
    defer close_ignored(output_pipe.write_end)

    argv: [3]cstr
    argv[0] = "tr"
    argv[1] = "a-z"
    argv[2] = "A-Z"

    child: os.Child
    child, err = os.spawn_pipe_argv("/usr/bin/tr", (Slice<cstr>)(argv), input_pipe, output_pipe)
    if !errors.is_ok(err) {
        return nil, err
    }

    close_ignored(input_pipe.read_end)
    close_ignored(output_pipe.write_end)

    err = write_exact(input_pipe.write_end, strings.bytes(line))
    if !errors.is_ok(err) {
        wait_ignored(&child)
        return nil, err
    }
    err = write_exact(input_pipe.write_end, strings.bytes("\n"))
    if !errors.is_ok(err) {
        wait_ignored(&child)
        return nil, err
    }
    err = io.close(input_pipe.write_end)
    if !errors.is_ok(err) {
        wait_ignored(&child)
        return nil, err
    }

    output: *Buffer<u8> = mem.buffer_new<u8>(alloc, line.len + 1)
    if output == nil {
        wait_ignored(&child)
        return nil, errors.fail_mem(RELAY_ERR_OUTPUT_ALLOC)
    }

    output_bytes: Slice<u8> = mem.slice_from_buffer<u8>(output)
    err = read_exact(output_pipe.read_end, output_bytes)
    if !errors.is_ok(err) {
        wait_ignored(&child)
        mem.buffer_free<u8>(output)
        return nil, err
    }
    err = io.close(output_pipe.read_end)
    if !errors.is_ok(err) {
        wait_ignored(&child)
        mem.buffer_free<u8>(output)
        return nil, err
    }

    status: i32
    status, err = os.wait(&child)
    if !errors.is_ok(err) {
        mem.buffer_free<u8>(output)
        return nil, err
    }
    if status != 0 {
        mem.buffer_free<u8>(output)
        return nil, errors.fail_os(RELAY_ERR_CHILD_EXIT)
    }

    return output, errors.ok()
}

func capture_invalid_tr_diagnostics(alloc: *mem.Allocator) (*Buffer<u8>, usize, i32, errors.Error) {
    input_pipe: io.Pipe
    err: errors.Error
    input_pipe, err = io.pipe()
    if !errors.is_ok(err) {
        return nil, 0, 0, err
    }
    defer close_ignored(input_pipe.read_end)
    defer close_ignored(input_pipe.write_end)

    output_pipe: io.Pipe
    output_pipe, err = io.pipe()
    if !errors.is_ok(err) {
        return nil, 0, 0, err
    }
    defer close_ignored(output_pipe.read_end)
    defer close_ignored(output_pipe.write_end)

    argv: [2]cstr
    argv[0] = "tr"
    argv[1] = "--definitely-invalid-option"

    child: os.Child
    child, err = os.spawn_pipe_argv_merged_output("/usr/bin/tr", (Slice<cstr>)(argv), input_pipe, output_pipe)
    if !errors.is_ok(err) {
        return nil, 0, 0, err
    }

    close_ignored(input_pipe.read_end)
    close_ignored(output_pipe.write_end)

    err = io.close(input_pipe.write_end)
    if !errors.is_ok(err) {
        wait_ignored(&child)
        return nil, 0, 0, err
    }

    output: *Buffer<u8> = mem.buffer_new<u8>(alloc, 256)
    if output == nil {
        wait_ignored(&child)
        return nil, 0, 0, errors.fail_mem(RELAY_ERR_DIAGNOSTIC_ALLOC)
    }

    output_bytes: Slice<u8> = mem.slice_from_buffer<u8>(output)
    used: usize
    used, err = read_until_eof(output_pipe.read_end, output_bytes)
    if !errors.is_ok(err) {
        wait_ignored(&child)
        mem.buffer_free<u8>(output)
        return nil, 0, 0, err
    }
    err = io.close(output_pipe.read_end)
    if !errors.is_ok(err) {
        wait_ignored(&child)
        mem.buffer_free<u8>(output)
        return nil, 0, 0, err
    }

    status: i32
    status, err = os.wait(&child)
    if !errors.is_ok(err) {
        mem.buffer_free<u8>(output)
        return nil, 0, 0, err
    }

    return output, used, status, errors.ok()
}

func capture_invalid_tr_split_diagnostics() (usize, usize, i32, errors.Error) {
    input_pipe: io.Pipe
    err: errors.Error
    input_pipe, err = io.pipe()
    if !errors.is_ok(err) {
        return 0, 0, 0, err
    }
    defer close_ignored(input_pipe.read_end)
    defer close_ignored(input_pipe.write_end)

    stdout_pipe: io.Pipe
    stdout_pipe, err = io.pipe()
    if !errors.is_ok(err) {
        return 0, 0, 0, err
    }
    defer close_ignored(stdout_pipe.read_end)
    defer close_ignored(stdout_pipe.write_end)

    stderr_pipe: io.Pipe
    stderr_pipe, err = io.pipe()
    if !errors.is_ok(err) {
        return 0, 0, 0, err
    }
    defer close_ignored(stderr_pipe.read_end)
    defer close_ignored(stderr_pipe.write_end)

    argv: [2]cstr
    argv[0] = "tr"
    argv[1] = "--definitely-invalid-option"

    child: os.Child
    child, err = os.spawn_pipe_argv_split_output("/usr/bin/tr", (Slice<cstr>)(argv), input_pipe, stdout_pipe, stderr_pipe)
    if !errors.is_ok(err) {
        return 0, 0, 0, err
    }

    close_ignored(input_pipe.read_end)
    close_ignored(stdout_pipe.write_end)
    close_ignored(stderr_pipe.write_end)

    err = io.close(input_pipe.write_end)
    if !errors.is_ok(err) {
        wait_ignored(&child)
        return 0, 0, 0, err
    }

    stdout_storage: [32]u8
    stdout_output: Slice<u8> = (Slice<u8>)(stdout_storage)
    stdout_used: usize
    stdout_used, err = read_until_eof(stdout_pipe.read_end, stdout_output)
    if !errors.is_ok(err) {
        wait_ignored(&child)
        return 0, 0, 0, err
    }
    err = io.close(stdout_pipe.read_end)
    if !errors.is_ok(err) {
        wait_ignored(&child)
        return 0, 0, 0, err
    }

    stderr_storage: [512]u8
    stderr_output: Slice<u8> = (Slice<u8>)(stderr_storage)
    stderr_used: usize
    stderr_used, err = read_until_eof(stderr_pipe.read_end, stderr_output)
    if !errors.is_ok(err) {
        wait_ignored(&child)
        return 0, 0, 0, err
    }
    err = io.close(stderr_pipe.read_end)
    if !errors.is_ok(err) {
        wait_ignored(&child)
        return 0, 0, 0, err
    }

    status: i32
    status, err = os.wait(&child)
    if !errors.is_ok(err) {
        return 0, 0, 0, err
    }

    return stdout_used, stderr_used, status, errors.ok()
}