import errors
import fmt
import io
import mem
import strings

@private
func is_ok(err: errors.Error) bool {
    return err == (errors.Error)(0)
}

@private
func report_failure(message: str) *i32 {
    _ = io.write_line(message)
    return fail()
}

@private
func print_i32_mismatch(label: str, actual: i32, expected: i32) *i32 {
    alloc: *mem.Allocator = mem.default_allocator()

    actual_buf: *Buffer<u8>
    expected_buf: *Buffer<u8>
    actual_err: errors.Error
    expected_err: errors.Error
    actual_buf, actual_err = fmt.sprint_i32(alloc, actual)
    expected_buf, expected_err = fmt.sprint_i32(alloc, expected)
    failed: bool = false
    if !is_ok(actual_err) {
        failed = true
    }
    if !is_ok(expected_err) {
        failed = true
    }
    if actual_buf == nil {
        failed = true
    }
    if expected_buf == nil {
        failed = true
    }
    if failed {
        if actual_buf != nil {
            mem.buffer_free<u8>(actual_buf)
        }
        if expected_buf != nil {
            mem.buffer_free<u8>(expected_buf)
        }
        return report_failure(label)
    }
    defer mem.buffer_free<u8>(actual_buf)
    defer mem.buffer_free<u8>(expected_buf)

    actual_bytes: Slice<u8> = mem.slice_from_buffer<u8>(actual_buf)
    expected_bytes: Slice<u8> = mem.slice_from_buffer<u8>(expected_buf)
    actual_text: str = str{ ptr: actual_bytes.ptr, len: actual_bytes.len }
    expected_text: str = str{ ptr: expected_bytes.ptr, len: expected_bytes.len }

    _ = io.write(label)
    _ = io.write(": got ")
    _ = io.write(actual_text)
    _ = io.write(", want ")
    _ = io.write_line(expected_text)
    return fail()
}

@private
func print_usize_mismatch(label: str, actual: usize, expected: usize) *i32 {
    alloc: *mem.Allocator = mem.default_allocator()

    actual_buf: *Buffer<u8>
    expected_buf: *Buffer<u8>
    actual_err: errors.Error
    expected_err: errors.Error
    actual_buf, actual_err = fmt.sprint_u64(alloc, (u64)(actual))
    expected_buf, expected_err = fmt.sprint_u64(alloc, (u64)(expected))
    failed: bool = false
    if !is_ok(actual_err) {
        failed = true
    }
    if !is_ok(expected_err) {
        failed = true
    }
    if actual_buf == nil {
        failed = true
    }
    if expected_buf == nil {
        failed = true
    }
    if failed {
        if actual_buf != nil {
            mem.buffer_free<u8>(actual_buf)
        }
        if expected_buf != nil {
            mem.buffer_free<u8>(expected_buf)
        }
        return report_failure(label)
    }
    defer mem.buffer_free<u8>(actual_buf)
    defer mem.buffer_free<u8>(expected_buf)

    actual_bytes: Slice<u8> = mem.slice_from_buffer<u8>(actual_buf)
    expected_bytes: Slice<u8> = mem.slice_from_buffer<u8>(expected_buf)
    actual_text: str = str{ ptr: actual_bytes.ptr, len: actual_bytes.len }
    expected_text: str = str{ ptr: expected_bytes.ptr, len: expected_bytes.len }

    _ = io.write(label)
    _ = io.write(": got ")
    _ = io.write(actual_text)
    _ = io.write(", want ")
    _ = io.write_line(expected_text)
    return fail()
}

extern(c) func __mc_testing_fail_sentinel() *i32

func fail() *i32 {
    return __mc_testing_fail_sentinel()
}

func expect(ok: bool) *i32 {
    if !ok {
        return report_failure("expect failed")
    }
    return nil
}

func expect_false(ok: bool) *i32 {
    if ok {
        return report_failure("expect_false failed")
    }
    return nil
}

func expect_i32_eq(actual: i32, expected: i32) *i32 {
    if actual != expected {
        return print_i32_mismatch("expect_i32_eq failed", actual, expected)
    }
    return nil
}

func expect_str_eq(actual: str, expected: str) *i32 {
    if !strings.eq(actual, expected) {
        _ = io.write("expect_str_eq failed: got ")
        _ = io.write(actual)
        _ = io.write(", want ")
        _ = io.write_line(expected)
        return fail()
    }
    return nil
}

func expect_usize_eq(actual: usize, expected: usize) *i32 {
    if actual != expected {
        return print_usize_mismatch("expect_usize_eq failed", actual, expected)
    }
    return nil
}