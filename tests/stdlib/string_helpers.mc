import strings

func main() i32 {
    if !strings.eq("mode", "mode") {
        return 1
    }
    if strings.eq("mode", "fast") {
        return 2
    }
    if strings.find_byte("port=8080", 61) != 4 {
        return 3
    }
    bytes: Slice<u8> = strings.bytes("port=8080")
    if bytes.len != 9 {
        return 4
    }
    if bytes[0] != 112 {
        return 5
    }
    if bytes[4] != 61 {
        return 6
    }

    trimmed: str = strings.trim_space(" \t fast\n")
    if !strings.eq(trimmed, "fast") {
        return 7
    }
    if !strings.is_blank(" \t\n") {
        return 8
    }
    if strings.is_blank(" value ") {
        return 9
    }
    return 0
}