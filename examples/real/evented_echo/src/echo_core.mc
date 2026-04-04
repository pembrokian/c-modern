export { parse_port }

func parse_port(text: str) (u16, uintptr) {
    if text.len == 0 {
        return 0, 1
    }

    bytes: Slice<u8> = Slice<u8>{ ptr: text.ptr, len: text.len }
    value: u32 = 0
    index: usize = 0
    while index < bytes.len {
        ch: u8 = bytes[index]
        if ch < 48 {
            return 0, 1
        }
        if ch > 57 {
            return 0, 1
        }
        value = value * (u32)(10) + (u32)(ch - 48)
        if value > (u32)(65535) {
            return 0, 1
        }
        index = index + 1
    }

    return (u16)(value), 0
}