struct Pair {
    left: i32,
    right: i32,
}

var global_pair: Pair

func bump(ptr: *i32) i32 {
    *ptr = *ptr + 1
    return *ptr
}

func main() i32 {
    local_pair: Pair = Pair{ left: 1, right: 2 }
    local_value: i32 = bump(&local_pair.left)
    global_value: i32 = bump(&global_pair.right)
    if local_pair.left != 2 {
        return 10
    }
    if global_pair.right != 1 {
        return 11
    }
    if local_value != 2 {
        return 12
    }
    if global_value != 1 {
        return 13
    }
    return 0
}