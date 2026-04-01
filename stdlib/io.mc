export { write, write_line }

extern(c) func __mc_io_write(text: str) i32
extern(c) func __mc_io_write_line(text: str) i32

func write(text: str) i32 {
    return __mc_io_write(text)
}

func write_line(text: str) i32 {
    return __mc_io_write_line(text)
}