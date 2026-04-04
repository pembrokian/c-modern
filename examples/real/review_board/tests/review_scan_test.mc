export { test_review_scan }

import review_scan
import testing

func test_review_scan() *i32 {
    sample: str = "O! parser hotfix\nC. docs cleanup\nO. cache pressure\nO! release blocker\n"
    if review_scan.count_open_items(sample) != 3 {
        return testing.fail()
    }
    if review_scan.count_closed_items(sample) != 1 {
        return testing.fail()
    }
    if review_scan.count_urgent_open_items(sample) != 2 {
        return testing.fail()
    }
    return nil
}