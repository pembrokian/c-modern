func expose(ptr: *i32) uintptr {
    return (uintptr)(ptr)
}

func roundtrip(value: i32) i32 {
    raw: uintptr = expose(&value)
    ptr: *i32 = (*i32)(raw)
    return *ptr
}