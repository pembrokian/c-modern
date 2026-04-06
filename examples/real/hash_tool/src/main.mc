import io
import hash

func main(args: Slice<cstr>) i32 {
    if args.len != 2 {
        if io.write_line("usage: hash-tool <path>") != 0 {
            return 1
        }
        return 64
    }

    return hash.run(args[1])
}