enum Result {
    Ok(value: i32),
    Err,
}

func dispatch(value: Result) i32 {
    switch value {
    case Result.Ok(v):
        return v
    default:
        return 2
    }
}
