export { main }

import utf8

func main() i32 {
    if utf8.byte_len("ascii") != 5 {
        return 90
    }
    if !utf8.empty("") {
        return 91
    }
    if utf8.empty("x") {
        return 92
    }
    return 0
}