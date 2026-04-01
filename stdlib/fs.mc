export { file_size, read_all, read_all_default }

import mem

extern(c) func __mc_fs_file_size(path: str) isize
extern(c) func __mc_fs_read_all(path: str, alloc: *mem.Allocator) *Buffer<u8>

func file_size(path: str) isize {
    return __mc_fs_file_size(path)
}

func read_all(path: str, alloc: *mem.Allocator) *Buffer<u8> {
    return __mc_fs_read_all(path, alloc)
}

func read_all_default(path: str) *Buffer<u8> {
    return read_all(path, mem.default_allocator())
}