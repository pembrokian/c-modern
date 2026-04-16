import expr
import fs
import mem

func main(args: Slice<cstr>) i32 {
    if args.len != 2 {
        return 64
    }

    alloc := mem.default_allocator()
    buf := fs.read_all(args[1], alloc)
    if buf == nil {
        return 92
    }
    defer mem.buffer_free<u8>(buf)

    bytes := mem.slice_from_buffer<u8>(buf)
    text := str{ ptr: bytes.ptr, len: bytes.len }
    return expr.normalize_text(text)
}