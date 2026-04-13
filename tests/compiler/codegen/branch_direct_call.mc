func inc(value: i32) i32 {
    return value + 1
}

func choose(flag: bool) i32 {
    if flag {
        return inc(4)
    }

    return inc(1)
}
