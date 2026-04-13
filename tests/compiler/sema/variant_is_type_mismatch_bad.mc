enum Option<T> {
    Some(value: T)
    None
}

func main() bool {
    value: Option<i32> = Option<i32>.Some(7)
    return value is Option<f64>.Some
}