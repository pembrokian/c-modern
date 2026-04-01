export { main }

import fs
import mem

func main(args: Slice<cstr>) i32 {
    if args.len < 2 {
        return 90
    }

    buf: *Buffer<u8> = fs.read_all_default(args[1])
    len: usize = mem.buffer_len(buf)
    if len != 7 {
        mem.buffer_free<u8>(buf)
        return 91
    }

    mem.buffer_free<u8>(buf)
    return 0
}