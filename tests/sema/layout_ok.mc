@packed
struct PackedPair {
    small: u8
    large: i32
}

@abi(c)
struct Header {
    tag: u8
    value: i32
    tail: u16
}

struct Envelope {
    header: Header
    payload: [4]u8
}

func main() i32 {
    return 0
}
