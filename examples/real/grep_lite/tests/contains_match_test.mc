export { test_contains_match }

import grep
import testing

func test_contains_match() *i32 {
    if !grep.contains("alphabet", "pha") {
        return testing.fail()
    }
    return nil
}
