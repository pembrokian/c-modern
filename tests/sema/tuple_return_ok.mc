func pair() (i32, i32) {
    return 1, 2
}

func sum() i32 {
    left: i32 = 0
    right: i32 = 0
    left, right = pair()
    return left + right
}