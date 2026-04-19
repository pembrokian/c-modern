func write_empty() i32 {
    return 10
}

func write_steady() i32 {
    return 20
}

func write_busy() i32 {
    return 30
}

func write_attention() i32 {
    return 40
}

const ROLLUP_WRITERS: [4]func() i32 = [4]func() i32{
    write_empty,
    write_steady,
    write_busy,
    write_attention,
}