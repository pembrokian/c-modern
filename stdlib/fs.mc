import errors
import mem

@private
extern(c) func __mc_fs_file_size(path: str, err: *errors.Error) isize
@private
extern(c) func __mc_fs_is_dir(path: str, err: *errors.Error) i32
@private
extern(c) func __mc_fs_list_dir_err(path: str, alloc: *mem.Allocator, err: *errors.Error) *Buffer<u8>
extern(c) func __mc_fs_list_dir(path: str, alloc: *mem.Allocator) *Buffer<u8>
@private
extern(c) func __mc_fs_read_all_err(path: str, alloc: *mem.Allocator, err: *errors.Error) *Buffer<u8>
extern(c) func __mc_fs_read_all(path: str, alloc: *mem.Allocator) *Buffer<u8>
@private
extern(c) func __mc_fs_write_all_err(path: str, bytes: Slice<u8>, err: *errors.Error) bool
@private
extern(c) func __mc_fs_read_exact_err(path: str, bytes: Slice<u8>, err: *errors.Error) bool

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

func list_dir_err(path: str, alloc: *mem.Allocator) (*Buffer<u8>, errors.Error) {
    err: errors.Error = errors.ok()
    buf: *Buffer<u8> = __mc_fs_list_dir_err(path, alloc, &err)
    return buf, err
}

func list_dir(path: str, alloc: *mem.Allocator) *Buffer<u8> {
    buf: *Buffer<u8>
    err: errors.Error
    buf, err = list_dir_err(path, alloc)
    if errors.is_err(err) {
        return nil
    }
    return buf
}

func read_all_err(path: str, alloc: *mem.Allocator) (*Buffer<u8>, errors.Error) {
    err: errors.Error = errors.ok()
    buf: *Buffer<u8> = __mc_fs_read_all_err(path, alloc, &err)
    return buf, err
}

func read_all(path: str, alloc: *mem.Allocator) *Buffer<u8> {
    buf: *Buffer<u8>
    err: errors.Error
    buf, err = read_all_err(path, alloc)
    if errors.is_err(err) {
        return nil
    }
    return buf
}

func write_all_err(path: str, bytes: Slice<u8>) errors.Error {
    err: errors.Error = errors.ok()
    ok: bool = __mc_fs_write_all_err(path, bytes, &err)
    if !ok && errors.is_ok(err) {
        return errors.fail_io(1)
    }
    return err
}

func write_all(path: str, bytes: Slice<u8>) bool {
    return errors.is_ok(write_all_err(path, bytes))
}

func read_exact_err(path: str, bytes: Slice<u8>) errors.Error {
    err: errors.Error = errors.ok()
    ok: bool = __mc_fs_read_exact_err(path, bytes, &err)
    if !ok && errors.is_ok(err) {
        return errors.fail_io(1)
    }
    return err
}

func read_exact(path: str, bytes: Slice<u8>) bool {
    return errors.is_ok(read_exact_err(path, bytes))
}