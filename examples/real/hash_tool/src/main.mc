import io
import hash

func main(args: Slice<cstr>) i32 {
    if args.len != 2 {
        status: i32 = io.write_line("usage: hash-tool <path>")
        if status != 0 {
            return status
        }
        return 64
    }

    return hash.run(args[1])
}