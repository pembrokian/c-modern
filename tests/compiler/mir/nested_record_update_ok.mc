struct Inner {
    value: i32
    flag: bool
}

struct Outer {
    left: i32
    inner: Inner
}

func rewrite(outer: Outer) Outer {
    return outer with { inner.value: 7 }
}