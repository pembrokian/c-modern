import io

func main(args: Slice<cstr>) i32 {
    if args.len < 2 {
        return 7
    }
    if io.write_line(args[1]) != 0 {
        return 1
    }
    return 0
}