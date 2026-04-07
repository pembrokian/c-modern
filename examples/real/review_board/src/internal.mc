import strings

@private
func line_is_open(bytes: Slice<u8>, start: usize, newline: usize) bool {
    if newline <= start {
        return false
    }
    return bytes[start] == 79
}

@private
func line_is_closed(bytes: Slice<u8>, start: usize, newline: usize) bool {
    if newline <= start {
        return false
    }
    return bytes[start] == 67
}

@private
func line_is_urgent_open(bytes: Slice<u8>, start: usize, newline: usize) bool {
    if newline <= start + 1 {
        return false
    }
    if bytes[start] != 79 {
        return false
    }
    return bytes[start + 1] == 33
}

func count_open_items(text: str) usize {
    bytes: Slice<u8> = strings.bytes(text)
    count: usize = 0
    start: usize = 0
    while start < text.len {
        newline: usize = start
        while newline < text.len {
            if bytes[newline] == 10 {
                break
            }
            newline = newline + 1
        }

        if line_is_open(bytes, start, newline) {
            count = count + 1
        }

        if newline == text.len {
            break
        }
        start = newline + 1
    }
    return count
}

func count_closed_items(text: str) usize {
    bytes: Slice<u8> = strings.bytes(text)
    count: usize = 0
    start: usize = 0
    while start < text.len {
        newline: usize = start
        while newline < text.len {
            if bytes[newline] == 10 {
                break
            }
            newline = newline + 1
        }

        if line_is_closed(bytes, start, newline) {
            count = count + 1
        }

        if newline == text.len {
            break
        }
        start = newline + 1
    }
    return count
}

func count_urgent_open_items(text: str) usize {
    bytes: Slice<u8> = strings.bytes(text)
    count: usize = 0
    start: usize = 0
    while start < text.len {
        newline: usize = start
        while newline < text.len {
            if bytes[newline] == 10 {
                break
            }
            newline = newline + 1
        }

        if line_is_urgent_open(bytes, start, newline) {
            count = count + 1
        }

        if newline == text.len {
            break
        }
        start = newline + 1
    }
    return count
}