export { expect, expect_false, expect_i32_eq, expect_str_eq, expect_usize_eq, fail }

import strings

func fail() *i32 {
    raw: uintptr = 1
    return (*i32)(raw)
}

func expect(ok: bool) *i32 {
    if !ok {
        return fail()
    }
    return nil
}

func expect_false(ok: bool) *i32 {
    if ok {
        return fail()
    }
    return nil
}

func expect_i32_eq(actual: i32, expected: i32) *i32 {
    if actual != expected {
        return fail()
    }
    return nil
}

func expect_str_eq(actual: str, expected: str) *i32 {
    if !strings.eq(actual, expected) {
        return fail()
    }
    return nil
}

func expect_usize_eq(actual: usize, expected: usize) *i32 {
    if actual != expected {
        return fail()
    }
    return nil
}