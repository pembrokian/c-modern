func dispatch(value: Result) i32 {
    switch value {
    case Result.Ok(v):
        return 1
    default:
        return 2
    }
}