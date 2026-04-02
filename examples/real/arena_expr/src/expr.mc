export { Expr, normalize_text, parse_text, token_count }

import io
import mem

const EXPR_NUMBER: i32 = 1
const EXPR_ADD: i32 = 2

struct Expr {
    kind: i32
    text: str
    left: *Expr
    right: *Expr
}

func is_digit(ch: u8) bool {
    if ch < 48 {
        return false
    }
    return ch <= 57
}

func skip_space(text: str, pos: *usize) {
    bytes: Slice<u8> = Slice<u8>{ ptr: text.ptr, len: text.len }
    while *pos < text.len {
        ch: u8 = bytes[*pos]
        if ch == 32 {
            *pos = *pos + 1
            continue
        }
        if ch == 9 {
            *pos = *pos + 1
            continue
        }
        if ch == 10 {
            *pos = *pos + 1
            continue
        }
        if ch == 13 {
            *pos = *pos + 1
            continue
        }
        break
    }
}

func token_count(text: str) usize {
    bytes: Slice<u8> = Slice<u8>{ ptr: text.ptr, len: text.len }
    pos: usize = 0
    count: usize = 0
    while pos < text.len {
        skip_space(text, &pos)
        if pos >= text.len {
            break
        }

        ch: u8 = bytes[pos]
        if is_digit(ch) {
            pos = pos + 1
            while pos < text.len {
                if !is_digit(bytes[pos]) {
                    break
                }
                pos = pos + 1
            }
            count = count + 1
            continue
        }

        if ch == 43 {
            count = count + 1
            pos = pos + 1
            continue
        }

        if ch == 40 {
            count = count + 1
            pos = pos + 1
            continue
        }

        if ch == 41 {
            count = count + 1
            pos = pos + 1
            continue
        }

        return 0
    }

    return count
}

func new_number(arena: mem.Arena, text: str) *Expr {
    node: *Expr = mem.arena_new<Expr>(arena)
    if node == nil {
        return nil
    }
    *node = Expr{ kind: EXPR_NUMBER, text: text, left: nil, right: nil }
    return node
}

func new_add(arena: mem.Arena, left: *Expr, right: *Expr) *Expr {
    node: *Expr = mem.arena_new<Expr>(arena)
    if node == nil {
        return nil
    }
    *node = Expr{ kind: EXPR_ADD, text: "", left: left, right: right }
    return node
}

func parse_number(arena: mem.Arena, text: str, pos: *usize) *Expr {
    bytes: Slice<u8> = Slice<u8>{ ptr: text.ptr, len: text.len }
    start: usize = *pos
    while *pos < text.len {
        if !is_digit(bytes[*pos]) {
            break
        }
        *pos = *pos + 1
    }
    if start == *pos {
        return nil
    }
    return new_number(arena, text[start:*pos])
}

func parse_primary(arena: mem.Arena, text: str, pos: *usize) *Expr {
    bytes: Slice<u8> = Slice<u8>{ ptr: text.ptr, len: text.len }
    skip_space(text, pos)
    if *pos >= text.len {
        return nil
    }

    if is_digit(bytes[*pos]) {
        return parse_number(arena, text, pos)
    }

    if bytes[*pos] != 40 {
        return nil
    }
    *pos = *pos + 1
    node: *Expr = parse_sum(arena, text, pos)
    if node == nil {
        return nil
    }
    skip_space(text, pos)
    if *pos >= text.len {
        return nil
    }
    if bytes[*pos] != 41 {
        return nil
    }
    *pos = *pos + 1
    return node
}

func parse_sum(arena: mem.Arena, text: str, pos: *usize) *Expr {
    bytes: Slice<u8> = Slice<u8>{ ptr: text.ptr, len: text.len }
    left: *Expr = parse_primary(arena, text, pos)
    if left == nil {
        return nil
    }

    while true {
        skip_space(text, pos)
        if *pos >= text.len {
            break
        }
        if bytes[*pos] != 43 {
            break
        }
        *pos = *pos + 1

        right: *Expr = parse_primary(arena, text, pos)
        if right == nil {
            return nil
        }

        left = new_add(arena, left, right)
        if left == nil {
            return nil
        }
    }

    return left
}

func parse_text(arena: mem.Arena, text: str) *Expr {
    if token_count(text) == 0 {
        return nil
    }

    pos: usize = 0
    root: *Expr = parse_sum(arena, text, &pos)
    if root == nil {
        return nil
    }
    skip_space(text, &pos)
    if pos != text.len {
        return nil
    }
    return root
}

func write_expr(expr: *Expr) i32 {
    node: Expr = *expr
    if node.kind == EXPR_NUMBER {
        return io.write(node.text)
    }

    status: i32 = io.write("(")
    if status != 0 {
        return status
    }
    status = write_expr(node.left)
    if status != 0 {
        return status
    }
    status = io.write("+")
    if status != 0 {
        return status
    }
    status = write_expr(node.right)
    if status != 0 {
        return status
    }
    return io.write(")")
}

func normalize_text(text: str) i32 {
    alloc: *mem.Allocator = mem.default_allocator()
    arena_cap: usize = text.len * 64 + 256
    arena: mem.Arena = mem.arena_init(alloc, arena_cap)
    defer mem.arena_deinit(arena)

    root: *Expr = parse_text(arena, text)
    if root == nil {
        return 90
    }

    status: i32 = write_expr(root)
    if status != 0 {
        return status
    }
    return io.write_line("")
}