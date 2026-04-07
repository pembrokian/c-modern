import rollup_model
import testing

func test_model_total_items() *i32 {
    summary: rollup_model.Summary = rollup_model.Summary{ open_items: 2, closed_items: 1, blocked_items: 1, priority_items: 1 }

    err: *i32 = testing.expect_usize_eq(rollup_model.total_items(summary), 4)
    if err != nil {
        return err
    }

    err = testing.expect(rollup_model.has_priority(summary))
    if err != nil {
        return err
    }

    err = testing.expect_false(rollup_model.has_priority(rollup_model.Summary{ open_items: 1, closed_items: 1, blocked_items: 0, priority_items: 0 }))
    if err != nil {
        return err
    }
    return nil
}