const EMPTY_BYTES: Slice<u8> = []
const EMPTY_CODES: [0]u8 = []

func zero_args() Slice<u8> {
    return []
}

func zero_codes() [0]u8 {
    return []
}

func call_len(items: Slice<u8>) usize {
    return items.len
}

func use() usize {
    local: Slice<u8> = []
    codes: [0]u8 = []
    return call_len([])
}