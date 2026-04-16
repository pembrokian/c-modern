func rewrite(outer: Outer) Outer {
    return outer with { inner.value: 7 }
}