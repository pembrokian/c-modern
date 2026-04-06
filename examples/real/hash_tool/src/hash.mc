export { format_line, hash_bytes, hex_u64, run }

import fs
import io
import mem
import strings

const HASH_SEED: u64 = 1469598103934665603
const HASH_MULTIPLIER: u64 = 1099511628211

func hash_bytes(bytes: Slice<u8>) u64 {
    hash: u64 = HASH_SEED
    index: usize = 0
    while index < bytes.len {
        hash = hash * HASH_MULTIPLIER
        hash = hash + (u64)(bytes[index])
        index = index + 1
    }
    return hash
}

func hex_digit(nibble: u64) u8 {
    if nibble < 10 {
        return (u8)(48 + nibble)
    }
    return (u8)(87 + nibble)
}

func hex_u64(value: u64) *Buffer<u8> {
    text: *Buffer<u8> = mem.buffer_new<u8>(mem.default_allocator(), 16)
    if text == nil {
        return nil
    }

    digits: Slice<u8> = mem.slice_from_buffer<u8>(text)
    remaining: u64 = value
    index: usize = 16
    while index > 0 {
        index = index - 1
        digits[index] = hex_digit(remaining % 16)
        remaining = remaining / 16
    }
    return text
}

func format_line(hash: u64, path: str) *Buffer<u8> {
    hex_buf: *Buffer<u8> = hex_u64(hash)
    if hex_buf == nil {
        return nil
    }
    defer mem.buffer_free<u8>(hex_buf)

    line: *Buffer<u8> = mem.buffer_new<u8>(mem.default_allocator(), 16 + 2 + path.len)
    if line == nil {
        return nil
    }

    hex_bytes: Slice<u8> = mem.slice_from_buffer<u8>(hex_buf)
    line_bytes: Slice<u8> = mem.slice_from_buffer<u8>(line)
    index: usize = 0
    while index < 16 {
        line_bytes[index] = hex_bytes[index]
        index = index + 1
    }

    line_bytes[16] = 32
    line_bytes[17] = 32
    path_bytes: Slice<u8> = strings.bytes(path)
    index = 0
    while index < path.len {
        line_bytes[18 + index] = path_bytes[index]
        index = index + 1
    }
    return line
}

func run(path: str) i32 {
    alloc: *mem.Allocator = mem.default_allocator()
    contents: *Buffer<u8> = fs.read_all(path, alloc)
    if contents == nil {
        return 92
    }
    defer mem.buffer_free<u8>(contents)

    bytes: Slice<u8> = mem.slice_from_buffer<u8>(contents)
    hash: u64 = hash_bytes(bytes)
    line: *Buffer<u8> = format_line(hash, path)
    if line == nil {
        return 93
    }
    defer mem.buffer_free<u8>(line)

    line_bytes: Slice<u8> = mem.slice_from_buffer<u8>(line)
    line_text: str = str{ ptr: line_bytes.ptr, len: line_bytes.len }
    if io.write_line(line_text) != 0 {
        return 1
    }
    return 0
}