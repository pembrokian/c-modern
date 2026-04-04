export { test_audit_pause }

import review_status
import testing

func test_audit_pause() *i32 {
    if !review_status.audit_should_pause_text("O! parser hotfix\nC. docs cleanup\nO. cache pressure\n") {
        return testing.fail()
    }
    if review_status.audit_should_pause_text("C! release notes\nO. docs cleanup\n") {
        return testing.fail()
    }
    return nil
}