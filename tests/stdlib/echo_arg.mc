export { main }

import io

func main(args: Slice<cstr>) i32 {
    if args.len < 2 {
        return 7
    }
    return io.write_line(args[1])
}