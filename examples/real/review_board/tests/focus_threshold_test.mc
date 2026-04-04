export { test_focus_threshold }

import review_status
import testing

func test_focus_threshold() *i32 {
    if review_status.focus_needs_escalation_text("O! parser hotfix\nO! runtime fix\nO. build cleanup\n") {
        return testing.fail()
    }
    if !review_status.focus_needs_escalation_text("O! parser hotfix\nO! runtime fix\nO! release blocker\n") {
        return testing.fail()
    }
    return nil
}