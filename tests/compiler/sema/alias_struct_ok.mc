type Count = i32

struct Pair {
    left: Count
    right: Count
}

type PairAlias = Pair

func sum(pair: PairAlias) Count {
    return pair.left + pair.right
}

func main() Count {
    pair: PairAlias = PairAlias{ left: 2, right: 5 }
    return sum(pair)
}