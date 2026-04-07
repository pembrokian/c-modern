import errors
import io
import line_filter_relay
import mem

func main(args: Slice<cstr>) i32 {
    if args.len != 2 {
        if io.write_line("usage: line-filter-relay <text>") != 0 {
            return 1
        }
        return 64
    }

    output: *Buffer<u8>
    err: errors.Error
    output, err = line_filter_relay.relay_line(args[1], mem.default_allocator())
    if !errors.is_ok(err) {
        return 65
    }
    defer mem.buffer_free<u8>(output)

    bytes: Slice<u8> = mem.slice_from_buffer<u8>(output)
    if io.write(str{ ptr: bytes.ptr, len: bytes.len }) != 0 {
        return 66
    }
    return 0
}