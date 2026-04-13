func cleanup() {
}

func walk(values: Slice<i32>) i32 {
    total: i32 = 0
    defer cleanup()
    for index, value in values {
        total = total + value
    }
    for idx in 0..3 {
        total = total + idx
    }
    return total
}