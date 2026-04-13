type File = i32

struct Event {
    file: File
    readable: bool
    writable: bool
    failed: bool
}

func first_file(events: Slice<Event>) File {
    return events[0].file
}
