struct Box<T> {
    value: T
}

func read(box: Box<i32>) i32 {
    return box.value
}