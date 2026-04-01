func view(values: [4]i32) i32 {
    slice: Slice<i32> = (Slice<i32>)(values)
    if slice.len == 4 {
        return 0
    }
    return 1
}

func main() i32 {
    return view([4]i32{ 1, 2, 3, 4 })
}