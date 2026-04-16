struct Ops {
    apply: func(i32, i32) i32,
}

func add(left: i32, right: i32) i32 {
    return left + right
}

func use_fn(fn: func(i32, i32) i32, left: i32, right: i32) i32 {
    return fn(left, right)
}

func main() i32 {
    ops: Ops = Ops{ apply: add }
    return use_fn(ops.apply, 20, 22)
}