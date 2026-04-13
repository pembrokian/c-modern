func hit(value: i32) {
}

func demo() i32 {
    value: i32 = 1
    defer hit(value)
    value = 2
    return 0
}