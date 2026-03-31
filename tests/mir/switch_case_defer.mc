func choose(flag: i32) i32 {
    switch flag {
    case 0:
        defer hit()
        return 10
    default:
        return 20
    }
}