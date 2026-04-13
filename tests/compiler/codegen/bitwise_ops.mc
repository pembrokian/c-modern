func mix(left: i32, right: i32, extra: i32) i32 {
    both: i32 = left & right
    either: i32 = both | extra
    return either ^ 1
}