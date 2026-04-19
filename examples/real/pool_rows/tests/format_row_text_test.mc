import mem
import pool_rows
import testing

func test_format_row_text() *i32 {
    pool: mem.Pool = mem.pool_init(mem.default_allocator(), pool_rows.ROW_SLOT_BYTES, 1)
    err: *i32 = testing.expect_usize_eq(mem.pool_available(pool), 1)
    if err != nil {
        return err
    }
    defer mem.pool_deinit(pool)

    slot: pool_rows.RowSlot = pool_rows.take_row(pool)
    err = testing.expect(slot != nil)
    if err != nil {
        return err
    }

    used: usize = pool_rows.format_row(slot, 3)
    err = testing.expect_usize_eq(used, 5)
    if err != nil {
        return err
    }
    err = testing.expect_str_eq(pool_rows.row_text(slot, used), "row-3")
    if err != nil {
        return err
    }
    err = testing.expect(pool_rows.return_row(pool, slot))
    if err != nil {
        return err
    }
    return nil
}