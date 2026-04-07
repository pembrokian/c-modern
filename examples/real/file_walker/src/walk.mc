import fs
import errors
import io
import mem
import path
import strings

func entry_is_dir(entry: str) bool {
    if entry.len == 0 {
        return false
    }

    bytes: Slice<u8> = strings.bytes(entry)
    return bytes[entry.len - 1] == 47
}

func entry_name(entry: str) str {
    return path.basename(entry)
}

func path_join(root: str, child: str) *Buffer<u8> {
    return path.join(mem.default_allocator(), root, child)
}

func walk_path(root: str) i32 {
    is_dir: i32
    err: errors.Error
    is_dir, err = fs.is_dir(root)
    if errors.is_err(err) {
        return 94
    }
    if is_dir == 0 {
        size: isize
        size, err = fs.file_size(root)
        if errors.is_err(err) {
            return 94
        }
        if size < 0 {
            return 94
        }
        if errors.is_err(io.write_line(root)) {
            return 1
        }
        return 0
    }

    listing_buf: *Buffer<u8> = fs.list_dir(root, mem.default_allocator())
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
            child_buf: *Buffer<u8> = path.join(mem.default_allocator(), root, path.basename(entry))
            if child_buf == nil {
                return 93
            }

            child_bytes: Slice<u8> = mem.slice_from_buffer<u8>(child_buf)
            child_path: str = str{ ptr: child_bytes.ptr, len: child_bytes.len }
            status: i32 = 0
            if entry_is_dir(entry) {
                status = walk_path(child_path)
            } else {
                if errors.is_err(io.write_line(child_path)) {
                    mem.buffer_free<u8>(child_buf)
                    return 1
                }
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
