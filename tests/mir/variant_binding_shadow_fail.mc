enum Result {
    Ok(value: i32),
    Err,
}

func bad(value: i32, result: Result) i32 {
    switch result {
    case Result.Ok(value):
        return value
    default:
        return value
    }
    return 0
}