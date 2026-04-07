enum Option<T> {
    Some(value: T)
    None
}

func make() Option<i32> {
    return Option<i32>.Some(7)
}