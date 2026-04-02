export { file_size, is_dir, list_dir, read_all, read_all_default }

import mem

extern(c) func __mc_fs_file_size(path: str) isize
extern(c) func __mc_fs_is_dir(path: str) i32
extern(c) func __mc_fs_list_dir(path: str, alloc: *mem.Allocator) *Buffer<u8>
extern(c) func __mc_fs_read_all(path: str, alloc: *mem.Allocator) *Buffer<u8>

func file_size(path: str) isize {
    return __mc_fs_file_size(path)
}

func is_dir(path: str) bool {
    return __mc_fs_is_dir(path) != 0
}

func list_dir(path: str, alloc: *mem.Allocator) *Buffer<u8> {
    return __mc_fs_list_dir(path, alloc)
}

func read_all(path: str, alloc: *mem.Allocator) *Buffer<u8> {
    return __mc_fs_read_all(path, alloc)
}

func read_all_default(path: str) *Buffer<u8> {
    return read_all(path, mem.default_allocator())
}