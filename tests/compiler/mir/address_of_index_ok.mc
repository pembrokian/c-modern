func bump(ptr: *i32) i32 {
    *ptr = *ptr + 1
    return *ptr
}

func main() i32 {
    values: [3]i32
    values[0] = 1
    values[1] = 4
    values[2] = 8
    middle: *i32 = &((values)[1])
    if bump(middle) != 5 {
        return 10
    }
    return values[1]
}