import errors
import io
import mem
import strings

@private
const FMT_ERR_ALLOC: usize = 1
@private
const FMT_ERR_SHORT_WRITE: usize = 2
@private
const ASCII_ZERO: u8 = 48
@private
const ASCII_MINUS: u8 = 45
@private
const I32_MIN_VALUE: i32 = -2147483648
@private
const I32_MIN_ABS: u64 = 2147483648

@private
func write_all(file: io.File, bytes: Slice<u8>) errors.Error {
    total: usize = 0
    while total < bytes.len {
        // Keep retrying until the full slice is written or the sink reports an error.
        written: usize
        err: errors.Error
        written, err = io.write_file(file, bytes[total:bytes.len])
        if !errors.is_ok(err) {
            return err
        }
        if written == 0 {
            return errors.fail_io(FMT_ERR_SHORT_WRITE)
        }
        total = total + written
    }
    return errors.ok()
}

@private
func copy_text(alloc: *mem.Allocator, text: str) (*Buffer<u8>, errors.Error) {
    buf: *Buffer<u8> = mem.buffer_new<u8>(alloc, text.len)
    if buf == nil {
        return nil, errors.fail_mem(FMT_ERR_ALLOC)
    }

    dst: Slice<u8> = mem.slice_from_buffer<u8>(buf)
    src: Slice<u8> = strings.bytes(text)
    index: usize = 0
    while index < text.len {
        dst[index] = src[index]
        index = index + 1
    }
    return buf, errors.ok()
}

@private
func decimal_len_u64(value: u64) usize {
    digits: usize = 1
    current: u64 = value
    while current >= 10 {
        current = current / 10
        digits = digits + 1
    }
    return digits
}

@private
func write_decimal_u64(dst: Slice<u8>, value: u64) usize {
    digits: usize = decimal_len_u64(value)
    index: usize = digits
    current: u64 = value
    while index > 0 {
        index = index - 1
        dst[index] = (u8)((u64)(ASCII_ZERO) + (current % 10))
        current = current / 10
    }
    return digits
}

func print(text: str) errors.Error {
    return io.write(text)
}

func fprint(file: io.File, text: str) errors.Error {
    return write_all(file, strings.bytes(text))
}

func sprint(alloc: *mem.Allocator, text: str) (*Buffer<u8>, errors.Error) {
    return copy_text(alloc, text)
}

func sprint_u64(alloc: *mem.Allocator, value: u64) (*Buffer<u8>, errors.Error) {
    digits: usize = decimal_len_u64(value)
    buf: *Buffer<u8> = mem.buffer_new<u8>(alloc, digits)
    if buf == nil {
        return nil, errors.fail_mem(FMT_ERR_ALLOC)
    }

    bytes: Slice<u8> = mem.slice_from_buffer<u8>(buf)
    write_decimal_u64(bytes, value)
    return buf, errors.ok()
}

func sprint_bool(alloc: *mem.Allocator, value: bool) (*Buffer<u8>, errors.Error) {
    if value {
        return copy_text(alloc, "true")
    }
    return copy_text(alloc, "false")
}

func sprint_i32(alloc: *mem.Allocator, value: i32) (*Buffer<u8>, errors.Error) {
    if value >= 0 {
        return sprint_u64(alloc, (u64)(value))
    }

    magnitude: u64
    if value == I32_MIN_VALUE {
        magnitude = I32_MIN_ABS
    } else {
        magnitude = (u64)(0 - value)
    }

    digits: usize = decimal_len_u64(magnitude)
    buf: *Buffer<u8> = mem.buffer_new<u8>(alloc, digits + 1)
    if buf == nil {
        return nil, errors.fail_mem(FMT_ERR_ALLOC)
    }

    bytes: Slice<u8> = mem.slice_from_buffer<u8>(buf)
    bytes[0] = ASCII_MINUS
    write_decimal_u64(bytes[1:bytes.len], magnitude)
    return buf, errors.ok()
}
