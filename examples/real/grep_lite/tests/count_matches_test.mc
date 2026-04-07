import grep
import testing

func test_count_matches() *i32 {
    if grep.count_matches("alpha\nbeta\nalpha beta\n", "alpha") != 2 {
        return testing.fail()
    }
    return nil
}
