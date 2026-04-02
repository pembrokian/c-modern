export { test_path_join }

import mem
import testing
import walk

func same_text(left: str, right: str) bool {
    if left.len != right.len {
        return false
    }

    left_bytes: Slice<u8> = Slice<u8>{ ptr: left.ptr, len: left.len }
    right_bytes: Slice<u8> = Slice<u8>{ ptr: right.ptr, len: right.len }
    index: usize = 0
    while index < left.len {
        if left_bytes[index] != right_bytes[index] {
            return false
        }
        index = index + 1
    }
    return true
}

func test_path_join() *i32 {
    joined: *Buffer<u8> = walk.path_join("root", "child.txt")
    if joined == nil {
        return testing.fail()
    }
    defer mem.buffer_free<u8>(joined)

    joined_bytes: Slice<u8> = mem.slice_from_buffer<u8>(joined)
    joined_text: str = str{ ptr: joined_bytes.ptr, len: joined_bytes.len }
    if !same_text(joined_text, "root/child.txt") {
        return testing.fail()
    }
    return nil
}