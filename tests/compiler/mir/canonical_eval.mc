enum Expr {
    Int(value: i32),
    Add(left: i32, right: i32),
    Div(left: i32, right: i32),
}

func eval(expr: Expr) i32 {
    switch expr {
    case Expr.Int(int_value):
        return int_value
    case Expr.Add(add_left, add_right):
        return add_left + add_right
    case Expr.Div(div_left, div_right):
        if div_right == 0 {
            return 0
        }
        return div_left / div_right
    default:
        return 0
    }
    return 0
}