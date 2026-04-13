func test() i32 {
    switch x {
        case mod.Color.Red:
            return 1
        case mod.Color.Blue(v):
            return v
    }
    return 0
}
