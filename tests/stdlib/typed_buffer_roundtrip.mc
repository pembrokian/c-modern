import mem

func main() i32 {
    alloc: *mem.Allocator = mem.default_allocator()
    buf = mem.buffer_new<i32>(alloc, 2)
    view = mem.slice_from_buffer<i32>(buf)
    view[0] = 11
    view[1] = 7
    if view.len != 2 {
        mem.buffer_free<i32>(buf)
        return 1
    }
    if view[0] != 11 {
        mem.buffer_free<i32>(buf)
        return 2
    }
    if view[1] != 7 {
        mem.buffer_free<i32>(buf)
        return 3
    }
    mem.buffer_free<i32>(buf)
    return 0
}