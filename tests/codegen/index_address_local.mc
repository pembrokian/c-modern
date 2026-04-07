func bump(ptr: *i32) i32 {
    *ptr = *ptr + 1
    return *ptr
}

func main() i32 {
    values: [3]i32
    values[0] = 1
    values[1] = 4
    values[2] = 8

    mid: *i32 = &values[1]
    tail: *i32 = &((values)[2])

    if bump(mid) != 5 {
        return 10
    }
    if bump(tail) != 9 {
        return 11
    }
    if values[1] != 5 {
        return 12
    }
    if values[2] != 9 {
        return 13
    }
    return 0
}