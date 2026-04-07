import rollup_model
import rollup_parse
import testing

func test_parse_summary() *i32 {
    summary: rollup_model.Summary = rollup_parse.summarize_text("O open parser lane\nB release blocker\nC docs cleanup\nO! urgent runtime check\n")

    err: *i32 = testing.expect_usize_eq(summary.open_items, 2)
    if err != nil {
        return err
    }
    err = testing.expect_usize_eq(summary.closed_items, 1)
    if err != nil {
        return err
    }
    err = testing.expect_usize_eq(summary.blocked_items, 1)
    if err != nil {
        return err
    }
    err = testing.expect_usize_eq(summary.priority_items, 1)
    if err != nil {
        return err
    }
    return nil
}