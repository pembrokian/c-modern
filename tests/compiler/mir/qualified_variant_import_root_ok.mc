import color_module

func get_value(c: color_module.Color) i32 {
    switch c {
        case color_module.Color.Red:
            return 1
        case color_module.Color.Green:
            return 2
        case color_module.Color.Blue:
            return 3
    }
    return 0
}