export { main }

import mem

func allocate_len(alloc: *mem.Allocator, cap: usize) usize {
    buf: *Buffer<u8> = mem.buffer_new<u8>(alloc, cap)
    if buf == nil {
        return 0
    }
    defer mem.buffer_free<u8>(buf)

    return mem.buffer_len(buf)
}

func main() i32 {
    alloc: *mem.Allocator = mem.default_allocator()
    if alloc == nil {
        return 90
    }

    first: usize = allocate_len(alloc, 4)
    second: usize = allocate_len(alloc, 9)
    if first != 4 {
        return 91
    }
    if second != 9 {
        return 92
    }

    return 0
}