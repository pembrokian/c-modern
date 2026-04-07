import fs

func main(args: Slice<cstr>) i32 {
    if args.len < 3 {
        return 90
    }

    if !fs.is_dir(args[1]) {
        return 91
    }
    if fs.is_dir(args[2]) {
        return 92
    }
    return 0
}