struct Pair {
    left: i32
    right: i32
}

func bad_unknown(pair: Pair) Pair {
    return pair with { extra: 1 }
}

func bad_positional(pair: Pair) Pair {
    return pair with { 1 }
}

func bad_duplicate(pair: Pair) Pair {
    return pair with { right: 2, right: 3 }
}