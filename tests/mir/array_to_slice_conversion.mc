func view(values: [4]i32) usize {
    slice: Slice<i32> = (Slice<i32>)(values)
    return slice.len
}