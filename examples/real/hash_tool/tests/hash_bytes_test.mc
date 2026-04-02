export { test_hash_bytes }

import hash
import testing

func test_hash_bytes() *i32 {
    text: str = "abc"
    bytes: Slice<u8> = Slice<u8>{ ptr: text.ptr, len: text.len }
    if hash.hash_bytes(bytes) != 12289845435839482867 {
        return testing.fail()
    }
    return nil
}