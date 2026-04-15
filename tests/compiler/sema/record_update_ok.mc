struct Inner {
    value: i32
}

struct Pair {
    left: i32
    right: Inner
}

func rewrite(pair: Pair) Pair {
    return pair with { right: { value: 7 } }
}

func read(pair: Pair) i32 {
    return rewrite(pair).right.value
}

func main() i32 {
    start: Pair = Pair{ left: 1, right: Inner{ value: 2 } }
    return read(start)
}