import rollup_model
import rollup_render
import testing

func test_render_kind() *i32 {
    err: *i32 = testing.expect_i32_eq(rollup_render.rollup_kind(rollup_model.Summary{ open_items: 0, closed_items: 0, blocked_items: 0, priority_items: 0 }), 0)
    if err != nil {
        return err
    }

    empty_cells := rollup_render.rollup_display_cells(rollup_model.Summary{ open_items: 0, closed_items: 0, blocked_items: 0, priority_items: 0 })
    err = testing.expect_usize_eq(usize(empty_cells[0]), 69)
    if err != nil {
        return err
    }
    err = testing.expect_usize_eq(usize(empty_cells[1]), 77)
    if err != nil {
        return err
    }
    err = testing.expect_usize_eq(usize(empty_cells[2]), 84)
    if err != nil {
        return err
    }
    err = testing.expect_usize_eq(usize(empty_cells[3]), 89)
    if err != nil {
        return err
    }

    err = testing.expect_i32_eq(rollup_render.rollup_kind(rollup_model.Summary{ open_items: 1, closed_items: 1, blocked_items: 1, priority_items: 1 }), 3)
    if err != nil {
        return err
    }

    attention_cells := rollup_render.rollup_display_cells(rollup_model.Summary{ open_items: 1, closed_items: 1, blocked_items: 1, priority_items: 1 })
    err = testing.expect_usize_eq(usize(attention_cells[0]), 65)
    if err != nil {
        return err
    }
    err = testing.expect_usize_eq(usize(attention_cells[1]), 84)
    if err != nil {
        return err
    }
    err = testing.expect_usize_eq(usize(attention_cells[2]), 84)
    if err != nil {
        return err
    }
    err = testing.expect_usize_eq(usize(attention_cells[3]), 78)
    if err != nil {
        return err
    }

    err = testing.expect_i32_eq(rollup_render.rollup_kind(rollup_model.Summary{ open_items: 2, closed_items: 0, blocked_items: 0, priority_items: 0 }), 2)
    if err != nil {
        return err
    }
    return nil
}