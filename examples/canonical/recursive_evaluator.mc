enum Expr {
    Int(value: i32),
    Add(left: *Expr, right: *Expr),
    Div(left: *Expr, right: *Expr),
}

func eval(expr: *Expr) i32 {
    current: Expr = *expr
    switch current {
    case Expr.Int(value):
        return value
    case Expr.Add(add_left, add_right):
        return eval(add_left) + eval(add_right)
    case Expr.Div(div_left, div_right):
        divisor: i32 = eval(div_right)
        if divisor == 0 {
            return 0
        }
        return eval(div_left) / divisor
    default:
        return 0
    }
    return 0
}

func main() i32 {
    eight: Expr = Expr.Int(8)
    two: Expr = Expr.Int(2)
    three: Expr = Expr.Int(3)
    quotient: Expr = Expr.Div(&eight, &two)
    root: Expr = Expr.Add(&quotient, &three)

    if eval(&root) == 7 {
        return 0
    }
    return 1
}