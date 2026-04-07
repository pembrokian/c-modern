import mem
import utf8

func main() i32 {
    ascii: Slice<u8> = Slice<u8>{ ptr: "ok".ptr, len: 2 }
    if !utf8.valid_bytes(ascii) {
        return 1
    }

    alloc: *mem.Allocator = mem.default_allocator()
    bad = mem.buffer_new<u8>(alloc, 2)
    view = mem.slice_from_buffer<u8>(bad)
    view[0] = 192
    view[1] = 175
    if utf8.valid_bytes(view) {
        mem.buffer_free<u8>(bad)
        return 2
    }
    mem.buffer_free<u8>(bad)
    return 0
}