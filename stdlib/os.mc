import errors
import io

struct Child {
    raw: uintptr
}

@private
extern(c) func __mc_os_spawnve(program: str, argv: Slice<cstr>, stdin_file: io.File, stdout_file: io.File, stderr_file: io.File, close_file0: io.File, close_file1: io.File, err: *errors.Error) Child
extern(c) func __mc_os_spawnv(program: str, argv: Slice<cstr>, stdin_file: io.File, stdout_file: io.File, close_file0: io.File, close_file1: io.File, err: *errors.Error) Child
extern(c) func __mc_os_spawn3(program: str, arg0: str, arg1: str, stdin_file: io.File, stdout_file: io.File, close_file0: io.File, close_file1: io.File, err: *errors.Error) Child
extern(c) func __mc_os_wait(child: *Child, err: *errors.Error) i32

@private
func spawn_argv_stderr(program: str, argv: Slice<cstr>, stdin_file: io.File, stdout_file: io.File, stderr_file: io.File, close_file0: io.File, close_file1: io.File) (Child, errors.Error) {
    err: errors.Error = errors.ok()
    child: Child = __mc_os_spawnve(program, argv, stdin_file, stdout_file, stderr_file, close_file0, close_file1, &err)
    return child, err
}

func spawn_argv(program: str, argv: Slice<cstr>, stdin_file: io.File, stdout_file: io.File, close_file0: io.File, close_file1: io.File) (Child, errors.Error) {
    err: errors.Error = errors.ok()
    child: Child = __mc_os_spawnv(program, argv, stdin_file, stdout_file, close_file0, close_file1, &err)
    return child, err
}

func spawn_pipe_argv(program: str, argv: Slice<cstr>, stdin_pipe: io.Pipe, stdout_pipe: io.Pipe) (Child, errors.Error) {
    return spawn_argv(program, argv, stdin_pipe.read_end, stdout_pipe.write_end, stdin_pipe.write_end, stdout_pipe.read_end)
}

func spawn_pipe_argv_merged_output(program: str, argv: Slice<cstr>, stdin_pipe: io.Pipe, output_pipe: io.Pipe) (Child, errors.Error) {
    return spawn_argv_stderr(program, argv, stdin_pipe.read_end, output_pipe.write_end, output_pipe.write_end, stdin_pipe.write_end, output_pipe.read_end)
}

func spawn_pipe_argv_split_output(program: str, argv: Slice<cstr>, stdin_pipe: io.Pipe, stdout_pipe: io.Pipe, stderr_pipe: io.Pipe) (Child, errors.Error) {
    return spawn_argv_stderr(program, argv, stdin_pipe.read_end, stdout_pipe.write_end, stderr_pipe.write_end, stdin_pipe.write_end, stdout_pipe.read_end)
}

func spawn(program: str, arg0: str, arg1: str, stdin_file: io.File, stdout_file: io.File, close_file0: io.File, close_file1: io.File) (Child, errors.Error) {
    argv: [3]cstr
    argv[0] = program
    argv[1] = arg0
    argv[2] = arg1
    return spawn_argv(program, (Slice<cstr>)(argv), stdin_file, stdout_file, close_file0, close_file1)
}

func wait(child: *Child) (i32, errors.Error) {
    err: errors.Error = errors.ok()
    status: i32 = __mc_os_wait(child, &err)
    return status, err
}