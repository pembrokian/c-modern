import io
import walk

func main(args: Slice<cstr>) i32 {
    if args.len != 2 {
        status: i32 = io.write_line("usage: file-walker <path>")
        if status != 0 {
            return status
        }
        return 64
    }

    return walk.run(args[1])
}