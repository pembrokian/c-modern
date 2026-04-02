export { Error, ok, fail_code, is_ok }

type Error = uintptr

func ok() Error {
    value: Error = (Error)(0)
    return value
}

func fail_code(code: usize) Error {
    value: Error = (Error)((uintptr)(code))
    return value
}

func is_ok(err: Error) bool {
    zero: Error = (Error)(0)
    return err == zero
}