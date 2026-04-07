import errors
import io

struct Child {
    raw: uintptr
}

extern(c) func __mc_os_spawn3(program: str, arg0: str, arg1: str, stdin_file: io.File, stdout_file: io.File, close_file0: io.File, close_file1: io.File, err: *errors.Error) Child
extern(c) func __mc_os_wait(child: *Child, err: *errors.Error) i32

func spawn(program: str, arg0: str, arg1: str, stdin_file: io.File, stdout_file: io.File, close_file0: io.File, close_file1: io.File) (Child, errors.Error) {
    err: errors.Error = errors.ok()
    child: Child = __mc_os_spawn3(program, arg0, arg1, stdin_file, stdout_file, close_file0, close_file1, &err)
    return child, err
}

func wait(child: *Child) (i32, errors.Error) {
    err: errors.Error = errors.ok()
    status: i32 = __mc_os_wait(child, &err)
    return status, err
}