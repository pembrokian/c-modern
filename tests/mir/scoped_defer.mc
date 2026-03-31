func inner() {
}

func nested(flag: bool) i32 {
    total: i32 = 0
    {
        defer inner()
        if flag {
            total = 10
        }
    }
    total = total + 1
    return total
}