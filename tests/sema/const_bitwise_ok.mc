const MASK: i32 = 6 & 3
const FLAGS: i32 = MASK | 8
const MIXED: i32 = FLAGS ^ 1

func main() i32 {
    return MIXED
}