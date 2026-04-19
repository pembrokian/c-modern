import io

func write_empty() i32 {
    if io.write_line("issue-rollup-empty") != 0 {
        return 1
    }
    return 0
}

func write_steady() i32 {
    if io.write_line("issue-rollup-steady") != 0 {
        return 1
    }
    return 0
}

func write_busy() i32 {
    if io.write_line("issue-rollup-busy") != 0 {
        return 1
    }
    return 0
}

func write_attention() i32 {
    if io.write_line("issue-rollup-attention") != 0 {
        return 1
    }
    return 0
}

const ROLLUP_WRITERS: [4]func() i32 = [4]func() i32{
    write_empty,
    write_steady,
    write_busy,
    write_attention,
}