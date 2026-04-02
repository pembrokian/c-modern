export { test_entry_name }

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

func test_entry_name() *i32 {
    if !same_text(walk.entry_name("nested/"), "nested") {
        return testing.fail()
    }
    if !same_text(walk.entry_name("alpha.txt"), "alpha.txt") {
        return testing.fail()
    }
    return nil
}