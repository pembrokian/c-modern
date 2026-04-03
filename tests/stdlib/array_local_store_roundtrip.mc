export { main }

func zero_bytes() [4]u8 {
    bytes: [4]u8
    bytes[0] = 0
    bytes[1] = 0
    bytes[2] = 0
    bytes[3] = 0
    return bytes
}

func main() i32 {
    bytes = zero_bytes()
    bytes[0] = 195
    bytes[1] = 169

    if bytes[0] != 195 {
        return 1
    }
    if bytes[1] != 169 {
        return 2
    }
    if bytes[2] != 0 {
        return 3
    }

    return 0
}