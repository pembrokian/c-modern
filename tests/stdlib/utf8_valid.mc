import utf8

func main() i32 {
    if !utf8.valid("") {
        return 1
    }
    if !utf8.valid("ascii") {
        return 2
    }
    if !utf8.valid("cafe") {
        return 3
    }
    if !utf8.valid("cafee") {
        return 4
    }
    if !utf8.valid("é") {
        return 5
    }
    if !utf8.valid("π") {
        return 6
    }
    return 0
}