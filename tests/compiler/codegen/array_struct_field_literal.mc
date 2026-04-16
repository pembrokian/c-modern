struct Packet {
    bytes: [2]u8
}

func inline_packet() Packet {
    return Packet{ bytes: [2]u8{ 7, 9 } }
}

func helper_bytes() [2]u8 {
    return [2]u8{ 5, 6 }
}

func helper_packet() Packet {
    return Packet{ bytes: helper_bytes() }
}

func main() i32 {
    first: Packet = inline_packet()
    second: Packet = helper_packet()
    if first.bytes[0] != 7 || first.bytes[1] != 9 {
        return 1
    }
    if second.bytes[0] != 5 || second.bytes[1] != 6 {
        return 2
    }
    return 0
}