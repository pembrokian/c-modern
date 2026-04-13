struct Error {
    code: i32
}

func parse() (Error, i32) {
    err: Error = Error{ code: 0 }
    return err, 0
}