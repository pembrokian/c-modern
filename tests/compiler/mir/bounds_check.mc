func at(values: [4]i32, idx: usize) i32 {
    return values[idx]
}

func window(values: [4]i32, start: usize, end: usize) Slice<i32> {
    return values[start:end]
}