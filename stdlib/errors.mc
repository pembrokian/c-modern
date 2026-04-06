export { Error, ErrorKind, ok, fail, fail_code, fail_errno, fail_io, fail_utf8, fail_mem, fail_os, fail_net, fail_sync, fail_user, is_ok, is_err, kind, code, kind_none, kind_generic, kind_errno, kind_io, kind_utf8, kind_mem, kind_os, kind_net, kind_sync, kind_user }

type Error = uintptr
type ErrorKind = usize

const ERROR_KIND_SCALE: usize = 65536
const ERROR_MAX_CODE: usize = 65535

const ERROR_KIND_NONE: ErrorKind = 0
const ERROR_KIND_GENERIC: ErrorKind = 1
const ERROR_KIND_ERRNO: ErrorKind = 2
const ERROR_KIND_IO: ErrorKind = 3
const ERROR_KIND_UTF8: ErrorKind = 4
const ERROR_KIND_MEM: ErrorKind = 5
const ERROR_KIND_OS: ErrorKind = 6
const ERROR_KIND_NET: ErrorKind = 7
const ERROR_KIND_SYNC: ErrorKind = 8
const ERROR_KIND_USER: ErrorKind = 9

func kind_none() ErrorKind {
    return ERROR_KIND_NONE
}

func kind_generic() ErrorKind {
    return ERROR_KIND_GENERIC
}

func kind_errno() ErrorKind {
    return ERROR_KIND_ERRNO
}

func kind_io() ErrorKind {
    return ERROR_KIND_IO
}

func kind_utf8() ErrorKind {
    return ERROR_KIND_UTF8
}

func kind_mem() ErrorKind {
    return ERROR_KIND_MEM
}

func kind_os() ErrorKind {
    return ERROR_KIND_OS
}

func kind_net() ErrorKind {
    return ERROR_KIND_NET
}

func kind_sync() ErrorKind {
    return ERROR_KIND_SYNC
}

func kind_user() ErrorKind {
    return ERROR_KIND_USER
}

func ok() Error {
    value: Error = (Error)(0)
    return value
}

func fail(kind: ErrorKind, err_code: usize) Error {
    if kind == ERROR_KIND_NONE {
        return ok()
    }
    if err_code == 0 {
        return ok()
    }

    raw_code: usize = err_code
    if raw_code > ERROR_MAX_CODE {
        raw_code = ERROR_MAX_CODE
    }

    raw_kind: usize = kind
    raw: usize = raw_kind * ERROR_KIND_SCALE + raw_code
    return (Error)((uintptr)(raw))
}

func fail_code(err_code: usize) Error {
    return fail(ERROR_KIND_GENERIC, err_code)
}

func fail_errno(err_code: usize) Error {
    return fail(ERROR_KIND_ERRNO, err_code)
}

func fail_io(err_code: usize) Error {
    return fail(ERROR_KIND_IO, err_code)
}

func fail_utf8(err_code: usize) Error {
    return fail(ERROR_KIND_UTF8, err_code)
}

func fail_mem(err_code: usize) Error {
    return fail(ERROR_KIND_MEM, err_code)
}

func fail_os(err_code: usize) Error {
    return fail(ERROR_KIND_OS, err_code)
}

func fail_net(err_code: usize) Error {
    return fail(ERROR_KIND_NET, err_code)
}

func fail_sync(err_code: usize) Error {
    return fail(ERROR_KIND_SYNC, err_code)
}

func fail_user(err_code: usize) Error {
    return fail(ERROR_KIND_USER, err_code)
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
        return ERROR_KIND_NONE
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