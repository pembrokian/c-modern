func tick() {
}

func iterate() i32 {
    for idx in 0..2 {
        defer tick()
    }
    return 0
}