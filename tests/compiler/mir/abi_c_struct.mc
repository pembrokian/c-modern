@abi(c)
struct Header {
    tag: u8
    value: i32
}

func main() i32 {
    header := Header{ tag: 1, value: 9 }
    return header.value
}