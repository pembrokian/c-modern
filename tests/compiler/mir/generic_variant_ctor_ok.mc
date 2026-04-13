enum Option<T> {
    Some(value: T)
    None
}

func read() i32 {
    current: Option<i32> = Option<i32>.Some(7)
    switch current {
    case Option.Some(value):
        return value
    default:
        return 0
    }
}