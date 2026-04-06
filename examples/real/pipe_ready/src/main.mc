export { main }

import errors
import io
import pipe_ready
import sync

const PIPE_READY_ERR_UNEXPECTED_EOF: usize = 1

func close_ignored(file: io.File) {
    ignored: errors.Error = io.close(file)
    if !errors.is_ok(ignored) {
        return
    }
}

func read_exact(file: io.File, bytes: Slice<u8>) errors.Error {
    total: usize = 0
    while total < bytes.len {
        nread: usize
        err: errors.Error
        nread, err = io.read(file, bytes[total:bytes.len])
        if !errors.is_ok(err) {
            return err
        }
        if nread == 0 {
            return errors.fail_io(PIPE_READY_ERR_UNEXPECTED_EOF)
        }
        total = total + nread
    }
    return errors.ok()
}

func main() i32 {
    conn: io.Pipe
    err: errors.Error
    conn, err = io.pipe()
    if !errors.is_ok(err) {
        return 10
    }

    poller: *io.Poller
    poller, err = io.poller_new()
    if !errors.is_ok(err) {
        close_ignored(conn.read_end)
        close_ignored(conn.write_end)
        return 11
    }
    defer io.poller_close(poller)

    err = io.poller_add(poller, conn.read_end, io.EVENT_READABLE)
    if !errors.is_ok(err) {
        close_ignored(conn.read_end)
        close_ignored(conn.write_end)
        return 12
    }

    pipe_ready.bind_runtime(conn.write_end)

    writer_status: i32 = 0
    thread: sync.Thread
    thread, err = sync.thread_spawn<i32>(pipe_ready.writer, &writer_status)
    if !errors.is_ok(err) {
        close_ignored(conn.read_end)
        close_ignored(conn.write_end)
        return 13
    }

    events: [1]io.Event
    ready: usize
    ready, err = io.poller_wait(poller, (Slice<io.Event>)(events), 2000)
    if !errors.is_ok(err) {
        join_err_on_wait: errors.Error = sync.thread_join(thread)
        if !errors.is_ok(join_err_on_wait) {
            close_ignored(conn.read_end)
            return 14
        }
        close_ignored(conn.read_end)
        return 15
    }
    if ready != 1 {
        err = sync.thread_join(thread)
        if !errors.is_ok(err) {
            close_ignored(conn.read_end)
            return 16
        }
        close_ignored(conn.read_end)
        return 17
    }

    event: io.Event = events[0]
    if event.file != conn.read_end {
        err = sync.thread_join(thread)
        if !errors.is_ok(err) {
            close_ignored(conn.read_end)
            return 18
        }
        close_ignored(conn.read_end)
        return 19
    }
    if !event.readable {
        err = sync.thread_join(thread)
        if !errors.is_ok(err) {
            close_ignored(conn.read_end)
            return 20
        }
        close_ignored(conn.read_end)
        return 21
    }
    if event.failed {
        err = sync.thread_join(thread)
        if !errors.is_ok(err) {
            close_ignored(conn.read_end)
            return 20
        }
        close_ignored(conn.read_end)
        return 21
    }

    storage: [32]u8
    received: Slice<u8> = (Slice<u8>)(storage)
    expected_len: usize = pipe_ready.message_len()
    if expected_len > received.len {
        err = sync.thread_join(thread)
        if !errors.is_ok(err) {
            close_ignored(conn.read_end)
            return 22
        }
        close_ignored(conn.read_end)
        return 23
    }

    err = read_exact(conn.read_end, received[0:expected_len])
    if !errors.is_ok(err) {
        join_err_on_read: errors.Error = sync.thread_join(thread)
        if !errors.is_ok(join_err_on_read) {
            close_ignored(conn.read_end)
            return 24
        }
        close_ignored(conn.read_end)
        return 25
    }

    err = sync.thread_join(thread)
    if !errors.is_ok(err) {
        close_ignored(conn.read_end)
        return 26
    }

    err = io.close(conn.read_end)
    if !errors.is_ok(err) {
        return 27
    }
    if writer_status != 0 {
        return 28
    }
    if !pipe_ready.message_matches(received[0:expected_len]) {
        return 29
    }

    if io.write_line("pipe-ready-ok") != 0 {
        return 30
    }
    return 0
}