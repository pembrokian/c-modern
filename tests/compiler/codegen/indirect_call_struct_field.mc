struct Ops {
    apply: func(i32, i32) i32,
}

func add(left: i32, right: i32) i32 {
    return left + right
}

func main() i32 {
    ops: Ops = Ops{ apply: add }
    return ops.apply(40, 2)
}