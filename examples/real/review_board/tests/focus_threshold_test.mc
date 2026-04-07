import review_status
import testing

func test_focus_threshold() *i32 {
    err: *i32 = testing.expect_false(review_status.focus_needs_escalation_text("O! parser hotfix\nO! runtime fix\nO. build cleanup\n"))
    if err != nil {
        return err
    }
    err = testing.expect(review_status.focus_needs_escalation_text("O! parser hotfix\nO! runtime fix\nO! release blocker\n"))
    if err != nil {
        return err
    }
    return nil
}