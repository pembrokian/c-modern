import mem
import pool_rows
import testing

func test_take_return_reuse() *i32 {
    pool: mem.Pool = mem.pool_init(mem.default_allocator(), pool_rows.ROW_SLOT_BYTES, 2)
    err: *i32 = testing.expect_usize_eq(mem.pool_available(pool), 2)
    if err != nil {
        return err
    }
    defer mem.pool_deinit(pool)

    first: pool_rows.RowSlot = pool_rows.take_row(pool)
    err = testing.expect(first != nil)
    if err != nil {
        return err
    }
    second: pool_rows.RowSlot = pool_rows.take_row(pool)
    err = testing.expect(second != nil)
    if err != nil {
        return err
    }
    third: pool_rows.RowSlot = pool_rows.take_row(pool)
    err = testing.expect(third == nil)
    if err != nil {
        return err
    }
    err = testing.expect_usize_eq(mem.pool_available(pool), 0)
    if err != nil {
        return err
    }
    err = testing.expect(pool_rows.return_row(pool, first))
    if err != nil {
        return err
    }
    err = testing.expect_false(pool_rows.return_row(pool, first))
    if err != nil {
        return err
    }
    reused: pool_rows.RowSlot = pool_rows.take_row(pool)
    err = testing.expect(reused != nil)
    if err != nil {
        return err
    }
    err = testing.expect(reused == first)
    if err != nil {
        return err
    }
    err = testing.expect(pool_rows.return_row(pool, second))
    if err != nil {
        return err
    }
    err = testing.expect(pool_rows.return_row(pool, reused))
    if err != nil {
        return err
    }
    err = testing.expect_usize_eq(mem.pool_available(pool), 2)
    if err != nil {
        return err
    }
    return nil
}