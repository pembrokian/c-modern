import fs
import errors

func main(args: Slice<cstr>) i32 {
    if args.len < 2 {
        return 90
    }

    size: isize
    err: errors.Error
    size, err = fs.file_size(args[1])
    if errors.is_err(err) {
        return 92
    }
    if size != 7 {
        return 91
    }
    return 0
}