export { fail }

func fail() *i32 {
    raw: uintptr = 1
    return (*i32)(raw)
}