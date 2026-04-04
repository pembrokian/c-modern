export { File, Event, first_file }

type File = i32

struct Event {
    file: File
    readable: u8
    writable: u8
    failed: u8
}

func first_file(events: Slice<Event>) File {
    return events[0].file
}
