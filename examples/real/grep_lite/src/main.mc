export { main }

import grep

func main(args: Slice<cstr>) i32 {
    if args.len < 3 {
        return 90
    }
    return grep.run(args[1], args[2])
}
