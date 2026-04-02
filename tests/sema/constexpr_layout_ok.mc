const WIDTH: usize = 2
const HEIGHT: usize = WIDTH + 2
var TOTAL: usize = WIDTH + HEIGHT

type ByteCount = usize

@abi(c)
struct Header {
    magic: u32
    bytes: [HEIGHT]u8
}

struct Envelope {
    header: Header
    extra: [WIDTH]u8
}

func main() i32 {
    return 0
}