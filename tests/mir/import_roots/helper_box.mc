struct Box<T> {
    value: T
}

func read_i32(box: Box<i32>) i32 {
    return box.value
}