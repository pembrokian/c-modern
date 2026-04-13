struct Event {
    file: i32
    readable: bool
    writable: bool
    failed: bool
}

func main() i32 {
    event: Event = Event{ file: 7, readable: true, writable: true, failed: false }
    if !event.readable {
        return 10
    }
    if !event.writable {
        return 11
    }
    if event.failed {
        return 12
    }
    if event.file != 7 {
        return 13
    }
    return 0
}