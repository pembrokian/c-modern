import fs

func main(args: Slice<cstr>) i32 {
    if args.len < 2 {
        return 90
    }

    size: isize = fs.file_size(args[1])
    if size != 7 {
        return 91
    }
    return 0
}