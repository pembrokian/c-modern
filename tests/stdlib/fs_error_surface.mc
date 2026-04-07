import errors
import fs

func main() i32 {
    size: isize
    is_dir: i32
    err: errors.Error

    size, err = fs.file_size("/definitely/not/present/mc_phase51_missing_file.txt")
    if errors.is_ok(err) {
        return 1
    }
    if size >= 0 {
        return 2
    }
    if errors.kind(err) != errors.kind_errno() {
        return 3
    }

    is_dir, err = fs.is_dir("/definitely/not/present/mc_phase51_missing_dir")
    if errors.is_ok(err) {
        return 4
    }
    if is_dir != 0 {
        return 5
    }
    if errors.kind(err) != errors.kind_errno() {
        return 6
    }

    return 0
}