export { main }

import mem
import utf8

func main() i32 {
    alloc: *mem.Allocator = mem.default_allocator()
    buf = mem.buffer_new<u8>(alloc, 2)
    view = mem.slice_from_buffer<u8>(buf)
    view[0] = 192
    view[1] = 175
    text = string{ ptr: view.ptr, len: view.len }
    if utf8.valid(text) {
        mem.buffer_free<u8>(buf)
        return 1
    }
    mem.buffer_free<u8>(buf)
    return 0
}