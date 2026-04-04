export { test_entry_name }

import testing
import walk

func test_entry_name() *i32 {
    err: *i32 = testing.expect_str_eq(walk.entry_name("nested/"), "nested")
    if err != nil {
        return err
    }
    err = testing.expect_str_eq(walk.entry_name("alpha.txt"), "alpha.txt")
    if err != nil {
        return err
    }
    return nil
}