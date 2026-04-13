enum Option<T> {
    Some(value: T)
    None
}

func main() i32 {
    value: Option<i32> = Option<i32>.Some(7)
    if value is Option.Some {
        return value.Some.0
    }
    return 0
}