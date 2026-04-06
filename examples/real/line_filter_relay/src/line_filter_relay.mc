export { relay_line }

import errors
import io
import mem
import os
import strings

const RELAY_ERR_SHORT_WRITE: usize = 1
const RELAY_ERR_UNEXPECTED_EOF: usize = 2
const RELAY_ERR_OUTPUT_ALLOC: usize = 3
const RELAY_ERR_CHILD_EXIT: usize = 4

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

    child: os.Child
    child, err = os.spawn("/usr/bin/tr", "a-z", "A-Z", input_pipe.read_end, output_pipe.write_end, input_pipe.write_end, output_pipe.read_end)
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