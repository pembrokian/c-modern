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

func test_parse_tree() *i32 {
    arena: mem.Arena = mem.arena_init(mem.default_allocator(), 1024)
    defer mem.arena_deinit(arena)

    root: *expr.Expr = expr.parse_text(arena, "1 + 2 + 3")
    test_err: *i32 = testing.expect_non_nil<expr.Expr>(root)
    if test_err != nil {
        return test_err
    }

    top: expr.Expr = *root
    test_err = testing.expect_non_nil<expr.Expr>(top.left)
    if test_err != nil {
        return test_err
    }
    test_err = testing.expect_non_nil<expr.Expr>(top.right)
    if test_err != nil {
        return test_err
    }
    right_ptr: *expr.Expr = top.right
    right_node: expr.Expr = *right_ptr
    if !same_text(right_node.text, "3") {
        return testing.fail()
    }
    left_ptr: *expr.Expr = top.left
    left_node: expr.Expr = *left_ptr
    test_err = testing.expect_non_nil<expr.Expr>(left_node.left)
    if test_err != nil {
        return test_err
    }
    return nil
}