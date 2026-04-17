func matches(actual: [4]u8, expected: [4]u8) bool {
    return actual == expected
}

func differs(actual: [4]u8, expected: [4]u8) bool {
    return actual != expected
}

func main() i32 {
    payload: [4]u8 = [4]u8{ 1, 2, 0, 0 }
    same: [4]u8 = [4]u8{ 1, 2, 0, 0 }
    other: [4]u8 = [4]u8{ 1, 9, 0, 0 }
    if !matches(payload, same) {
        return 1
    }
    if differs(payload, same) {
        return 2
    }
    if matches(payload, other) {
        return 3
    }
    if !differs(payload, other) {
        return 4
    }
    return 0
}