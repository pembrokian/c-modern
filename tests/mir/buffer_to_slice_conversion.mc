func view(values: Buffer<i32>) usize {
    slice: Slice<i32> = (Slice<i32>)(values)
    return slice.len
}