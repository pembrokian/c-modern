const ROWS: usize = 2
const COLS: usize = 3

func row_sum(values: [COLS]i32) i32 {
    return values[0] + values[1] + values[2]
}

func grid_sum(values: [ROWS][COLS]i32) i32 {
    return row_sum(values[0]) + row_sum(values[1])
}

func main() i32 {
    weights: [ROWS][COLS]i32 = [ROWS][COLS]i32{
        [COLS]i32{ 1, 2, 3 },
        [COLS]i32{ 4, 5, 6 },
    }
    if weights[1][2] != 6 {
        return 91
    }
    if row_sum(weights[0]) != 6 {
        return 92
    }
    if grid_sum(weights) != 21 {
        return 93
    }
    return 0
}
