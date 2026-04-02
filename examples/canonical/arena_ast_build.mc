import mem

struct Token {
    kind: i32
    value: i32
}

struct Expr {
    kind: i32
    value: i32
    left: *Expr
    right: *Expr
}

func parse_primary(arena: mem.Arena, tokens: Slice<Token>, pos: *usize) *Expr {
    token: Token = tokens[*pos]
    *pos = *pos + 1

    node: *Expr = mem.arena_new<Expr>(arena)
    if node == nil {
        return node
    }

    *node = Expr{ kind: 1, value: token.value, left: nil, right: nil }
    return node
}

func parse_sum(arena: mem.Arena, tokens: Slice<Token>, pos: *usize) *Expr {
    left: *Expr = parse_primary(arena, tokens, pos)
    if left == nil {
        return left
    }

    while *pos < tokens.len {
        token: Token = tokens[*pos]
        if token.kind != 1 {
            break
        }
        *pos = *pos + 1

        right: *Expr = parse_primary(arena, tokens, pos)
        if right == nil {
            return right
        }

        parent: *Expr = mem.arena_new<Expr>(arena)
        if parent == nil {
            return parent
        }

        *parent = Expr{ kind: 2, value: 0, left: left, right: right }
        left = parent
    }

    return left
}

func eval(expr: *Expr) i32 {
    node: Expr = *expr
    if node.kind == 1 {
        return node.value
    }
    return eval(node.left) + eval(node.right)
}

func main() i32 {
    alloc: *mem.Allocator = mem.default_allocator()
    arena: mem.Arena = mem.arena_init(alloc, 1024)
    defer mem.arena_deinit(arena)

    token_buf: *Buffer<Token> = mem.buffer_new<Token>(alloc, 5)
    if token_buf == nil {
        return 90
    }
    defer mem.buffer_free<Token>(token_buf)

    tokens: Slice<Token> = mem.slice_from_buffer<Token>(token_buf)
    tokens[0] = Token{ kind: 0, value: 1 }
    tokens[1] = Token{ kind: 1, value: 0 }
    tokens[2] = Token{ kind: 0, value: 2 }
    tokens[3] = Token{ kind: 1, value: 0 }
    tokens[4] = Token{ kind: 0, value: 3 }

    pos: usize = 0
    root: *Expr = parse_sum(arena, tokens, &pos)
    if root == nil {
        return 91
    }
    if eval(root) != 6 {
        return 92
    }

    top: Expr = *root
    if top.left == nil {
        return 93
    }
    if top.right == nil {
        return 94
    }
    if eval(top.left) != 3 {
        return 95
    }
    return 0
}