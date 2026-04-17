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

const ROUTES: Routes = Routes{
    first: inc,
    second: dec
}
