export { main }

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

    trimmed: str = strings.trim_space(" \t fast\n")
    if !strings.eq(trimmed, "fast") {
        return 4
    }
    if !strings.is_blank(" \t\n") {
        return 5
    }
    if strings.is_blank(" value ") {
        return 6
    }
    return 0
}