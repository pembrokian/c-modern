export { main }

import mem

func main() i32 {
    alloc: *mem.Allocator = mem.default_allocator()
    buf = mem.buffer_new<u8>(alloc, 5)
    if mem.buffer_len(buf) != 5 {
        mem.buffer_free<u8>(buf)
        return 1
    }
    mem.buffer_free<u8>(buf)
    return 0
}