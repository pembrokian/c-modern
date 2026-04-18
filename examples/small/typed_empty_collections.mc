func empty_args() Slice<cstr> {
    return []
}

func main(args: Slice<cstr>) i32 {
    none: Slice<cstr> = []
    empty_codes: [0]u8 = []
    if none.len != 0 {
        return 1
    }
    if empty_args().len != 0 {
        return 2
    }
    return 0
}