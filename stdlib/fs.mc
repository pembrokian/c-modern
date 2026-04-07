import errors
import mem

@private
extern(c) func __mc_fs_file_size(path: str, err: *errors.Error) isize
@private
extern(c) func __mc_fs_is_dir(path: str, err: *errors.Error) i32
// TODO: widen list_dir to surface errors.Error once a real admitted consumer
// needs actionable filesystem failure diagnostics rather than nil-only failure.
extern(c) func __mc_fs_list_dir(path: str, alloc: *mem.Allocator) *Buffer<u8>
// TODO: widen read_all to surface errors.Error once a real admitted consumer
// needs actionable filesystem failure diagnostics rather than nil-only failure.
extern(c) func __mc_fs_read_all(path: str, alloc: *mem.Allocator) *Buffer<u8>

func file_size(path: str) (isize, errors.Error) {
    err: errors.Error = errors.ok()
    size: isize = __mc_fs_file_size(path, &err)
    return size, err
}

func is_dir(path: str) (i32, errors.Error) {
    err: errors.Error = errors.ok()
    raw: i32 = __mc_fs_is_dir(path, &err)
    return raw, err
}

func list_dir(path: str, alloc: *mem.Allocator) *Buffer<u8> {
    return __mc_fs_list_dir(path, alloc)
}

func read_all(path: str, alloc: *mem.Allocator) *Buffer<u8> {
    return __mc_fs_read_all(path, alloc)
}