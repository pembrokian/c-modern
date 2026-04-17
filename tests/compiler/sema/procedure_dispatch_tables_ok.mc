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

func apply_local(value: u8) u8 {
    local_ops: [2]func(u8) u8 = [2]func(u8) u8{ dec, inc }
    return local_ops[1](value)
}

func main() i32 {
    return i32(OPS[0](40)) + i32(ROUTES.second(3)) + i32(apply_local(1))
}