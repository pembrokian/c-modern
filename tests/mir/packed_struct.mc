@packed
struct PackedPair {
    small: u8
    large: i32
}

func main() i32 {
    pair = PackedPair{ small: 1, large: 9 }
    return pair.large
}