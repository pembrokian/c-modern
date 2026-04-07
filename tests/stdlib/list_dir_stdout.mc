import fs
import io
import mem
import errors

func main(args: Slice<cstr>) i32 {
    if args.len < 2 {
        return 90
    }

    alloc: *mem.Allocator = mem.default_allocator()
    entries: *Buffer<u8> = fs.list_dir(args[1], alloc)
    if entries == nil {
        return 91
    }
    defer mem.buffer_free<u8>(entries)

    bytes: Slice<u8> = mem.slice_from_buffer<u8>(entries)
    text: str = str{ ptr: bytes.ptr, len: bytes.len }
    if errors.is_err(io.write(text)) {
        return 1
    }
    return 0
}