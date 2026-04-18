const BASE: usize = 4
const WIDTH: usize = BASE + 1

struct Buffer {
    data: [WIDTH]u8,
}

var SCRATCH: [WIDTH]u8

func copy(data: [WIDTH]u8) [WIDTH]u8 {
    return data
}
