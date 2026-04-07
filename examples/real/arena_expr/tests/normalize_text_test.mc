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

func test_normalize_text() *i32 {
    arena: mem.Arena = mem.arena_init(mem.default_allocator(), 2048)
    defer mem.arena_deinit(arena)

    root: *expr.Expr = expr.parse_text(arena, "1 + (2 + 3)")
    if root == nil {
        return testing.fail()
    }

    root_node: expr.Expr = *root
    if root_node.left == nil {
        return testing.fail()
    }
    if root_node.right == nil {
        return testing.fail()
    }
    left_ptr: *expr.Expr = root_node.left
    left_node: expr.Expr = *left_ptr
    if !same_text(left_node.text, "1") {
        return testing.fail()
    }
    right_ptr: *expr.Expr = root_node.right
    right_node: expr.Expr = *right_ptr
    if right_node.left == nil {
        return testing.fail()
    }
    return nil
}