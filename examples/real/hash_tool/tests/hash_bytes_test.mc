export { test_hash_bytes }

import hash
import strings
import testing

func test_hash_bytes() *i32 {
    text: str = "abc"
    bytes: Slice<u8> = strings.bytes(text)
    if hash.hash_bytes(bytes) != 12289845435839482867 {
        return testing.fail()
    }
    return nil
}