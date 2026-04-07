import strings

extern(c) func __mc_testing_fail_sentinel() *i32

func fail() *i32 {
    return __mc_testing_fail_sentinel()
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