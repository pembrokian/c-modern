func inc(value: u8) u8 {
    return value + 1
}

func dec(value: u8) u8 {
    return value - 1
}

struct Routes {
    first: func(u8) u8,
    second: func(u8) u8,
}

const OPS: [2]func(u8) u8 = [2]func(u8) u8{
    inc,
    dec
}

const ROUTES: Routes = Routes{
    first: inc,
    second: dec
}

func main() i32 {
    local_ops: [2]func(u8) u8 = [2]func(u8) u8{ dec, inc }
    op: func(u8) u8 = OPS[0]
    return i32(op(40)) + i32(ROUTES.second(3)) + i32(local_ops[1](1))
}