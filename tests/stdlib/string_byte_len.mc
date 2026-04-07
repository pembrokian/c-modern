import strings

func main() i32 {
    if "phase6".len != 6 {
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