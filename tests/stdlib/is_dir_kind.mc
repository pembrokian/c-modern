import fs
import errors

func main(args: Slice<cstr>) i32 {
    if args.len < 3 {
        return 90
    }

    dir_ok: i32
    err: errors.Error
    dir_ok, err = fs.is_dir(args[1])
    if errors.is_err(err) {
        return 93
    }
    if dir_ok == 0 {
        return 91
    }

    dir_ok, err = fs.is_dir(args[2])
    if errors.is_err(err) {
        return 94
    }
    if dir_ok != 0 {
        return 92
    }
    return 0
}