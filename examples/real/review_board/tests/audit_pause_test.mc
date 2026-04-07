import review_status
import testing

func test_audit_pause() *i32 {
    err: *i32 = testing.expect(review_status.audit_should_pause_text("O! parser hotfix\nC. docs cleanup\nO. cache pressure\n"))
    if err != nil {
        return err
    }
    err = testing.expect_false(review_status.audit_should_pause_text("C! release notes\nO. docs cleanup\n"))
    if err != nil {
        return err
    }
    return nil
}