import errors

type File = i32

struct Pipe {
    read_end: File
    write_end: File
}

type EventMask = u32

const EVENT_READABLE: EventMask = 1
const EVENT_WRITABLE: EventMask = 2

struct Event {
    file: File
    readable: bool
    writable: bool
    failed: bool
}

struct Poller {
    raw: uintptr
}

@private
extern(c) func __mc_io_write(text: str) errors.Error
@private
extern(c) func __mc_io_write_line(text: str) errors.Error
@private
extern(c) func __mc_io_read(file: File, bytes: Slice<u8>, err: *errors.Error) usize
@private
extern(c) func __mc_io_write_file(file: File, bytes: Slice<u8>, err: *errors.Error) usize
@private
extern(c) func __mc_io_close(file: File) errors.Error
@private
extern(c) func __mc_io_pipe(err: *errors.Error) Pipe
@private
extern(c) func __mc_io_poller_new(err: *errors.Error) *Poller
@private
extern(c) func __mc_io_poller_add(p: *Poller, file: File, interests: EventMask) errors.Error
@private
extern(c) func __mc_io_poller_set(p: *Poller, file: File, interests: EventMask) errors.Error
@private
extern(c) func __mc_io_poller_remove(p: *Poller, file: File) errors.Error
@private
extern(c) func __mc_io_poller_wait(p: *Poller, events: *Event, cap: usize, timeout_ms: i32, err: *errors.Error) usize
@private
extern(c) func __mc_io_poller_close(p: *Poller)

func write(text: str) errors.Error {
    return __mc_io_write(text)
}

func write_line(text: str) errors.Error {
    return __mc_io_write_line(text)
}

func read(file: File, bytes: Slice<u8>) (usize, errors.Error) {
    err: errors.Error = errors.ok()
    count: usize = __mc_io_read(file, bytes, &err)
    return count, err
}

func write_file(file: File, bytes: Slice<u8>) (usize, errors.Error) {
    err: errors.Error = errors.ok()
    count: usize = __mc_io_write_file(file, bytes, &err)
    return count, err
}

func close(file: File) errors.Error {
    return __mc_io_close(file)
}

func pipe() (Pipe, errors.Error) {
    err: errors.Error = errors.ok()
    conn: Pipe = __mc_io_pipe(&err)
    return conn, err
}

func poller_new() (*Poller, errors.Error) {
    err: errors.Error = errors.ok()
    poller: *Poller = __mc_io_poller_new(&err)
    return poller, err
}

func poller_add(p: *Poller, file: File, interests: EventMask) errors.Error {
    return __mc_io_poller_add(p, file, interests)
}

func poller_set(p: *Poller, file: File, interests: EventMask) errors.Error {
    return __mc_io_poller_set(p, file, interests)
}

func poller_remove(p: *Poller, file: File) errors.Error {
    return __mc_io_poller_remove(p, file)
}

func poller_wait(p: *Poller, events: Slice<Event>, timeout_ms: i32) (usize, errors.Error) {
    err: errors.Error = errors.ok()
    count: usize = __mc_io_poller_wait(p, events.ptr, events.len, timeout_ms, &err)
    return count, err
}

func poller_close(p: *Poller) {
    __mc_io_poller_close(p)
}
