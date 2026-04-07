import strings

const WRITE_LIMIT: usize = 128

func fill_response(bytes: Slice<u8>) {
    pattern: Slice<u8> = strings.bytes("phase16-partial-write-state|")
    index: usize = 0
    while index < bytes.len {
        bytes[index] = pattern[index % pattern.len]
        index = index + 1
    }
}

func write_chunk_len(remaining_len: usize) usize {
    if remaining_len > WRITE_LIMIT {
        return WRITE_LIMIT
    }
    return remaining_len
}