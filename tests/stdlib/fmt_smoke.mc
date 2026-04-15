import errors
import fmt
import io
import mem
import strings

func main() i32 {
    if fmt.print("fmt") != 0 {
        return 1
    }

    alloc: *mem.Allocator = mem.default_allocator()

    text_buf: *Buffer<u8>
    err: errors.Error
    text_buf, err = fmt.sprint(alloc, "123")
    if !errors.is_ok(err) {
        return 2
    }
    if text_buf == nil {
        return 3
    }
    defer mem.buffer_free<u8>(text_buf)

    text_bytes: Slice<u8> = mem.slice_from_buffer<u8>(text_buf)
    text: str = str{ ptr: text_bytes.ptr, len: text_bytes.len }
    if !strings.eq(text, "123") {
        return 4
    }

    num_buf: *Buffer<u8>
    num_buf, err = fmt.sprint_i32(alloc, i32(-42))
    if !errors.is_ok(err) {
        return 5
    }
    if num_buf == nil {
        return 6
    }
    defer mem.buffer_free<u8>(num_buf)

    num_bytes: Slice<u8> = mem.slice_from_buffer<u8>(num_buf)
    num_text: str = str{ ptr: num_bytes.ptr, len: num_bytes.len }
    if !strings.eq(num_text, "-42") {
        return 7
    }

    bool_buf: *Buffer<u8>
    bool_buf, err = fmt.sprint_bool(alloc, true)
    if !errors.is_ok(err) {
        return 8
    }
    if bool_buf == nil {
        return 9
    }
    defer mem.buffer_free<u8>(bool_buf)

    bool_bytes: Slice<u8> = mem.slice_from_buffer<u8>(bool_buf)
    bool_text: str = str{ ptr: bool_bytes.ptr, len: bool_bytes.len }
    if !strings.eq(bool_text, "true") {
        return 10
    }

    pipe: io.Pipe
    pipe_err: errors.Error
    pipe, pipe_err = io.pipe()
    if !errors.is_ok(pipe_err) {
        return 11
    }
    defer io.close(pipe.read_end)

    if fmt.fprint(pipe.write_end, "ok") != 0 {
        return 12
    }
    if io.close(pipe.write_end) != 0 {
        return 13
    }

    bytes: [2]u8
    count: usize
    read_err: errors.Error
    count, read_err = io.read(pipe.read_end, (Slice<u8>)(bytes))
    if !errors.is_ok(read_err) {
        return 14
    }
    if count != 2 {
        return 15
    }
    if bytes[0] != 111 {
        return 16
    }
    if bytes[1] != 107 {
        return 17
    }

    return 0
}
