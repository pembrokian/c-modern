struct WindowStats {
    total: i32,
    first: i32,
    count: usize,
}

func summarize(values: [4]i32, start: usize, end: usize) WindowStats {
    slice: Slice<i32> = (Slice<i32>)(values)
    window: Slice<i32> = slice[start:end]
    total: i32 = 0
    for index, value in window {
        total = total + value
    }
    return WindowStats{ total: total, first: window[0], count: window.len }
}