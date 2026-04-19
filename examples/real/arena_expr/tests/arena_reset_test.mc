import expr
import mem
import testing

func same_text(left: str, right: str) bool {
    if left.len != right.len {
        return false
    }

    left_bytes: Slice<u8> = Slice<u8>{ ptr: left.ptr, len: left.len }
    right_bytes: Slice<u8> = Slice<u8>{ ptr: right.ptr, len: right.len }
    index: usize = 0
    while index < left.len {
        if left_bytes[index] != right_bytes[index] {
            return false
        }
        index = index + 1
    }
    return true
}

func test_arena_reset_reuses_scratch() *i32 {
    arena: mem.Arena = mem.arena_init(mem.default_allocator(), 1024)
    defer mem.arena_deinit(arena)

    first: *expr.Expr = expr.parse_text(arena, "1 + 2")
    err: *i32 = testing.expect_non_nil<expr.Expr>(first)
    if err != nil {
        return err
    }

    mem.arena_reset(arena)

    second: *expr.Expr = expr.parse_text(arena, "7 + (8 + 9)")
    err = testing.expect_non_nil<expr.Expr>(second)
    if err != nil {
        return err
    }

    root: expr.Expr = *second
    err = testing.expect_non_nil<expr.Expr>(root.left)
    if err != nil {
        return err
    }
    err = testing.expect_non_nil<expr.Expr>(root.right)
    if err != nil {
        return err
    }

    left_ptr: *expr.Expr = root.left
    left_node: expr.Expr = *left_ptr
    if !same_text(left_node.text, "7") {
        return testing.fail()
    }

    right_ptr: *expr.Expr = root.right
    right_node: expr.Expr = *right_ptr
    err = testing.expect_non_nil<expr.Expr>(right_node.left)
    if err != nil {
        return err
    }
    err = testing.expect_non_nil<expr.Expr>(right_node.right)
    if err != nil {
        return err
    }

    right_right_ptr: *expr.Expr = right_node.right
    right_right: expr.Expr = *right_right_ptr
    if !same_text(right_right.text, "9") {
        return testing.fail()
    }
    return nil
}