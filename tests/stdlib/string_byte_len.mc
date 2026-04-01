export { main }

import strings

func main() i32 {
    if strings.byte_len("phase6") != 6 {
        return 1
    }
    if !strings.empty("") {
        return 2
    }
    if strings.empty("x") {
        return 3
    }
    return 0
}