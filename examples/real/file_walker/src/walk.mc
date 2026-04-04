export { entry_is_dir, entry_name, path_join, run }

import fs
import io
import mem
import strings

func entry_is_dir(entry: str) bool {
    if entry.len == 0 {
        return false
    }

    bytes: Slice<u8> = strings.bytes(entry)
    return bytes[entry.len - 1] == 47
}

func entry_name(entry: str) str {
    if entry_is_dir(entry) {
        return entry[0:entry.len - 1]
    }
    return entry
}

func path_join(base: str, name: str) *Buffer<u8> {
    base_bytes: Slice<u8> = strings.bytes(base)
    name_bytes: Slice<u8> = strings.bytes(name)
    need_separator: bool = false
    if base.len > 0 {
        need_separator = base_bytes[base.len - 1] != 47
    }

    extra: usize = 0
    if need_separator {
        extra = 1
    }

    joined: *Buffer<u8> = mem.buffer_new<u8>(mem.default_allocator(), base.len + extra + name.len)
    if joined == nil {
        return nil
    }

    bytes: Slice<u8> = mem.slice_from_buffer<u8>(joined)
    cursor: usize = 0
    while cursor < base.len {
        bytes[cursor] = base_bytes[cursor]
        cursor = cursor + 1
    }
    if need_separator {
        bytes[cursor] = 47
        cursor = cursor + 1
    }

    index: usize = 0
    while index < name.len {
        bytes[cursor + index] = name_bytes[index]
        index = index + 1
    }
    return joined
}

func walk_path(path: str) i32 {
    if !fs.is_dir(path) {
        if fs.file_size(path) < 0 {
            return 94
        }
        return io.write_line(path)
    }

    listing_buf: *Buffer<u8> = fs.list_dir(path, mem.default_allocator())
    if listing_buf == nil {
        return 92
    }
    defer mem.buffer_free<u8>(listing_buf)

    listing_bytes: Slice<u8> = mem.slice_from_buffer<u8>(listing_buf)
    listing: str = str{ ptr: listing_bytes.ptr, len: listing_bytes.len }
    start: usize = 0
    while start < listing.len {
        newline: usize = start
        while newline < listing.len {
            if listing_bytes[newline] == 10 {
                break
            }
            newline = newline + 1
        }

        entry: str = listing[start:newline]
        if entry.len != 0 {
            child_buf: *Buffer<u8> = path_join(path, entry_name(entry))
            if child_buf == nil {
                return 93
            }

            child_bytes: Slice<u8> = mem.slice_from_buffer<u8>(child_buf)
            child_path: str = str{ ptr: child_bytes.ptr, len: child_bytes.len }
            status: i32 = 0
            if entry_is_dir(entry) {
                status = walk_path(child_path)
            } else {
                status = io.write_line(child_path)
            }
            mem.buffer_free<u8>(child_buf)
            if status != 0 {
                return status
            }
        }

        if newline == listing.len {
            break
        }
        start = newline + 1
    }

    return 0
}

func run(root: str) i32 {
    return walk_path(root)
}