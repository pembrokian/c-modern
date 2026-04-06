import io
import walk

func main(args: Slice<cstr>) i32 {
    if args.len != 2 {
        if io.write_line("usage: file-walker <path>") != 0 {
            return 1
        }
        return 64
    }

    return walk.run(args[1])
}