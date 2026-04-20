import issue_rollup_interactive
import rollup_render
import testing

func expect_cells(cells: [4]u8, b0: usize, b1: usize, b2: usize, b3: usize) *i32 {
    err: *i32 = testing.expect_usize_eq(usize(cells[0]), b0)
    if err != nil {
        return err
    }
    err = testing.expect_usize_eq(usize(cells[1]), b1)
    if err != nil {
        return err
    }
    err = testing.expect_usize_eq(usize(cells[2]), b2)
    if err != nil {
        return err
    }
    return testing.expect_usize_eq(usize(cells[3]), b3)
}

func test_interactive_app() *i32 {
    state := issue_rollup_interactive.issue_rollup_interactive_init()

    result := issue_rollup_interactive.issue_rollup_handle_key(state, 79)
    err: *i32 = testing.expect(result.changed)
    if err != nil {
        return err
    }
    err = testing.expect_usize_eq(result.summary.open_items, 1)
    if err != nil {
        return err
    }
    err = expect_cells(rollup_render.rollup_display_cells(result.summary), 83, 84, 68, 89)
    if err != nil {
        return err
    }

    result = issue_rollup_interactive.issue_rollup_handle_key(result.state, 79)
    err = testing.expect(result.changed)
    if err != nil {
        return err
    }
    err = testing.expect_usize_eq(result.summary.open_items, 2)
    if err != nil {
        return err
    }
    err = expect_cells(rollup_render.rollup_display_cells(result.summary), 66, 85, 83, 89)
    if err != nil {
        return err
    }

    result = issue_rollup_interactive.issue_rollup_handle_key(result.state, 80)
    err = testing.expect(result.changed)
    if err != nil {
        return err
    }
    err = testing.expect_usize_eq(result.summary.priority_items, 1)
    if err != nil {
        return err
    }
    err = expect_cells(rollup_render.rollup_display_cells(result.summary), 65, 84, 84, 78)
    if err != nil {
        return err
    }

    result = issue_rollup_interactive.issue_rollup_handle_key(result.state, 82)
    err = testing.expect(result.changed)
    if err != nil {
        return err
    }
    err = testing.expect_usize_eq(result.summary.open_items, 0)
    if err != nil {
        return err
    }
    err = testing.expect_usize_eq(result.summary.priority_items, 0)
    if err != nil {
        return err
    }
    return expect_cells(rollup_render.rollup_display_cells(result.summary), 69, 77, 84, 89)
}