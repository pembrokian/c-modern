import bundle_stage
import io

func main(args: Slice<cstr>) i32 {
    if args.len != 2 {
        if io.write_line("usage: bundle-stage <path>") != 0 {
            return 1
        }
        return 64
    }

    return bundle_stage.run(args[1])
}