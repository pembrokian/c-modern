import fs
import mem

func main(args: Slice<cstr>) i32 {
    if args.len < 2 {
        return 90
    }

    alloc := mem.default_allocator()
    buf := fs.read_all(args[1], alloc)
    len := mem.buffer_len(buf)
    if len == 0 {
        mem.buffer_free<u8>(buf)
        return 91
    }
    if len != 7 {
        mem.buffer_free<u8>(buf)
        return 92
    }

    mem.buffer_free<u8>(buf)
    return 7
}