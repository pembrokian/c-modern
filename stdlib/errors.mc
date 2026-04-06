export { Error, ErrorKind, ok, fail, fail_code, from_errno, fail_errno, fail_io, fail_utf8, fail_mem, fail_os, fail_net, fail_sync, fail_user, is_ok, is_err, kind, code, kind_none, kind_generic, kind_errno, kind_io, kind_utf8, kind_mem, kind_os, kind_net, kind_sync, kind_user }

type Error = uintptr
type ErrorKind = usize

const ERROR_KIND_SCALE: usize = 65536

func kind_none() ErrorKind {
    return 0
}

func kind_generic() ErrorKind {
    return 1
}

func kind_errno() ErrorKind {
    return 2
}

func kind_io() ErrorKind {
    return 3
}

func kind_utf8() ErrorKind {
    return 4
}

func kind_mem() ErrorKind {
    return 5
}

func kind_os() ErrorKind {
    return 6
}

func kind_net() ErrorKind {
    return 7
}

func kind_sync() ErrorKind {
    return 8
}

func kind_user() ErrorKind {
    return 9
}

func ok() Error {
    value: Error = (Error)(0)
    return value
}

func fail(kind: ErrorKind, err_code: usize) Error {
    if kind == kind_none() {
        return ok()
    }
    if err_code == 0 {
        return ok()
    }

    raw_kind: usize = kind
    raw: usize = raw_kind * ERROR_KIND_SCALE + err_code
    return (Error)((uintptr)(raw))
}

func fail_code(err_code: usize) Error {
    return fail(kind_generic(), err_code)
}

func from_errno(err_code: usize) Error {
    return fail(kind_errno(), err_code)
}

func fail_errno(err_code: usize) Error {
    return from_errno(err_code)
}

func fail_io(err_code: usize) Error {
    return fail(kind_io(), err_code)
}

func fail_utf8(err_code: usize) Error {
    return fail(kind_utf8(), err_code)
}

func fail_mem(err_code: usize) Error {
    return fail(kind_mem(), err_code)
}

func fail_os(err_code: usize) Error {
    return fail(kind_os(), err_code)
}

func fail_net(err_code: usize) Error {
    return fail(kind_net(), err_code)
}

func fail_sync(err_code: usize) Error {
    return fail(kind_sync(), err_code)
}

func fail_user(err_code: usize) Error {
    return fail(kind_user(), err_code)
}

func is_ok(err: Error) bool {
    zero: Error = (Error)(0)
    return err == zero
}

func is_err(err: Error) bool {
    return !is_ok(err)
}

func kind(err: Error) ErrorKind {
    if is_ok(err) {
        return kind_none()
    }
    raw: usize = (usize)(err)
    return raw / ERROR_KIND_SCALE
}

func code(err: Error) usize {
    if is_ok(err) {
        return 0
    }
    raw: usize = (usize)(err)
    raw_kind: usize = kind(err)
    return raw - raw_kind * ERROR_KIND_SCALE
}