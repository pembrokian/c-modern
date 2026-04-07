import grep
import testing

func test_contains_miss() *i32 {
    if grep.contains("alphabet", "zzz") {
        return testing.fail()
    }
    return nil
}
