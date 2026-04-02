export { test_entry_is_dir }

import testing
import walk

func test_entry_is_dir() *i32 {
    if !walk.entry_is_dir("nested/") {
        return testing.fail()
    }
    if walk.entry_is_dir("alpha.txt") {
        return testing.fail()
    }
    return nil
}