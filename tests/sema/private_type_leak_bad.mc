@private
struct Hidden {
    value: i32
}

func leak(value: Hidden) Hidden {
    return value
}