import io

func main(args: Slice<cstr>) i32 {
    if io.write_line("hello from stdlib") != 0 {
        return 1
    }
    return 0
}