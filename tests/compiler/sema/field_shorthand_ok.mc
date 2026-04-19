struct Inner {
    value: i32
    flag: bool
}

struct Pair {
    left: i32
    inner: Inner
}

func build(left: i32, value: i32, flag: bool) Pair {
    inner := Inner{ value:, flag: }
    return Pair{ left:, inner: }
}

func rewrite(left: i32, pair: Pair) Pair {
    return pair with { left: }
}

func main() i32 {
    pair := build(1, 2, true)
    next := rewrite(7, pair)
    if next.left == 7 && next.inner.value == 2 && next.inner.flag {
        return 0
    }
    return 1
}