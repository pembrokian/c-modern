import mem

func main() i32 {
    alloc: *mem.Allocator = mem.default_allocator()
    buf = mem.buffer_new<u8>(alloc, 7)
    view = mem.slice_from_buffer<u8>(buf)
    if view.len != 7 {
        mem.buffer_free<u8>(buf)
        return 1
    }
    mem.buffer_free<u8>(buf)
    return 0
}