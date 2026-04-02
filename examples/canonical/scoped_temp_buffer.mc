export { main }

import mem

func main() i32 {
    alloc: *mem.Allocator = mem.default_allocator()
    total: i32 = 0

    for idx in 0..3 {
        buf = mem.buffer_new<u8>(alloc, idx + 1)
        defer mem.buffer_free<u8>(buf)

        if mem.buffer_len(buf) != idx + 1 {
            return 90
        }

        total = total + 2
    }

    return total
}