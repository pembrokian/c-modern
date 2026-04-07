import mem

func sum_fixed(values: [4]i32) i32 {
    view: Slice<i32> = (Slice<i32>)(values)
    return view[0] + view[1] + view[2] + view[3]
}

func sum_buffer(values: Buffer<i32>) i32 {
    view: Slice<i32> = (Slice<i32>)(values)
    if view.len != 3 {
        return 90
    }
    return view[0] + view[1] + view[2]
}

func main() i32 {
    alloc: *mem.Allocator = mem.default_allocator()

    fixed: [4]i32
    fixed[0] = 1
    fixed[1] = 2
    fixed[2] = 3
    fixed[3] = 4
    if sum_fixed(fixed) != 10 {
        return 91
    }

    buf: *Buffer<i32> = mem.buffer_new<i32>(alloc, 3)
    if buf == nil {
        return 92
    }
    defer mem.buffer_free<i32>(buf)

    writable: Slice<i32> = mem.slice_from_buffer<i32>(buf)
    writable[0] = 5
    writable[1] = 6
    writable[2] = 7

    if writable.len != 3 {
        return 93
    }
    if sum_buffer(*buf) != 18 {
        return 94
    }

    return 0
}