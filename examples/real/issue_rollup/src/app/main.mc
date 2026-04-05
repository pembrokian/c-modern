export { main }

import fs
import mem
import rollup_core

func main(args: Slice<cstr>) i32 {
    if args.len != 2 {
        return 64
    }

    buf: *Buffer<u8> = fs.read_all(args[1], mem.default_allocator())
    if buf == nil {
        return 92
    }
    defer mem.buffer_free<u8>(buf)

    bytes: Slice<u8> = mem.slice_from_buffer<u8>(buf)
    text: str = str{ ptr: bytes.ptr, len: bytes.len }
    return rollup_core.write_text_rollup(text)
}