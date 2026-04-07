import mem
import path
import strings
import time

func main() i32 {
    max_u64: u64 = (u64)(-1)

    joined_buf: *Buffer<u8> = path.join(mem.default_allocator(), "root", "child.txt")
    if joined_buf == nil {
        return 1
    }
    defer mem.buffer_free<u8>(joined_buf)

    joined_bytes: Slice<u8> = mem.slice_from_buffer<u8>(joined_buf)
    joined_text: str = str{ ptr: joined_bytes.ptr, len: joined_bytes.len }
    if !strings.eq(joined_text, "root/child.txt") {
        return 2
    }
    if !strings.eq(path.basename(joined_text), "child.txt") {
        return 3
    }
    if !strings.eq(path.dirname(joined_text), "root") {
        return 4
    }

    if !strings.eq(path.basename(""), ".") {
        return 8
    }
    if !strings.eq(path.dirname(""), ".") {
        return 9
    }
    if !strings.eq(path.basename("/"), "/") {
        return 10
    }
    if !strings.eq(path.dirname("/"), "/") {
        return 11
    }
    if !strings.eq(path.basename("root/child///"), "child") {
        return 12
    }
    if !strings.eq(path.dirname("root/child///"), "root") {
        return 13
    }
    if !strings.eq(path.dirname("child.txt"), ".") {
        return 14
    }
    if !strings.eq(path.dirname("root//child"), "root") {
        return 15
    }

    empty_join: *Buffer<u8> = path.join(mem.default_allocator(), "", "")
    if empty_join == nil {
        return 16
    }
    defer mem.buffer_free<u8>(empty_join)
    empty_joined_bytes: Slice<u8> = mem.slice_from_buffer<u8>(empty_join)
    if empty_joined_bytes.len != 0 {
        return 17
    }

    absolute_join: *Buffer<u8> = path.join(mem.default_allocator(), "root", "/child.txt")
    if absolute_join == nil {
        return 18
    }
    defer mem.buffer_free<u8>(absolute_join)
    absolute_joined_bytes: Slice<u8> = mem.slice_from_buffer<u8>(absolute_join)
    absolute_joined_text: str = str{ ptr: absolute_joined_bytes.ptr, len: absolute_joined_bytes.len }
    if !strings.eq(absolute_joined_text, "/child.txt") {
        return 19
    }

    one_ms: time.Duration = time.millis((u64)(1))
    two_s: time.Duration = time.seconds((u64)(2))
    combined: time.Duration = time.add(one_ms, two_s)
    if time.nanos(one_ms) != (u64)(1000000) {
        return 5
    }
    if time.nanos(combined) != (u64)(2001000000) {
        return 6
    }

    overflow_seconds: time.Duration = time.seconds(max_u64)
    if time.nanos(overflow_seconds) != max_u64 {
        return 20
    }

    near_max: time.Duration = time.from_nanos(max_u64 - (u64)(5))
    saturated_sum: time.Duration = time.add(near_max, time.from_nanos((u64)(9)))
    if time.nanos(saturated_sum) != max_u64 {
        return 21
    }

    start: time.Duration = time.monotonic()
    finish: time.Duration = time.monotonic()
    if time.less(finish, start) {
        return 7
    }

    return 0
}
