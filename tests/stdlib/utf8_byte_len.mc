import utf8

func main() i32 {
    if "ascii".len != 5 {
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