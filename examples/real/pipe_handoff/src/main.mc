import errors
import io
import pipe_handoff
import sync

const PIPE_HANDOFF_ERR_UNEXPECTED_EOF: usize = 1

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
            return errors.fail_io(PIPE_HANDOFF_ERR_UNEXPECTED_EOF)
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

    pipe_handoff.bind_runtime(conn.write_end)

    writer_status: i32 = 0
    thread: sync.Thread
    thread, err = sync.thread_spawn<i32>(pipe_handoff.writer, &writer_status)
    if !errors.is_ok(err) {
        close_ignored(conn.read_end)
        close_ignored(conn.write_end)
        return 11
    }

    storage: [32]u8
    received: Slice<u8> = (Slice<u8>)(storage)
    expected_len: usize = pipe_handoff.message_len()
    if expected_len > received.len {
        err = sync.thread_join(thread)
        if !errors.is_ok(err) {
            close_ignored(conn.read_end)
            return 12
        }
        close_ignored(conn.read_end)
        return 13
    }

    err = read_exact(conn.read_end, received[0:expected_len])
    if !errors.is_ok(err) {
        join_err: errors.Error = sync.thread_join(thread)
        if !errors.is_ok(join_err) {
            close_ignored(conn.read_end)
            return 14
        }
        close_ignored(conn.read_end)
        return 15
    }

    err = sync.thread_join(thread)
    if !errors.is_ok(err) {
        close_ignored(conn.read_end)
        return 16
    }

    err = io.close(conn.read_end)
    if !errors.is_ok(err) {
        return 17
    }
    if writer_status != 0 {
        return 18
    }
    if !pipe_handoff.message_matches(received[0:expected_len]) {
        return 19
    }

    if io.write_line("pipe-handoff-ok") != 0 {
        return 20
    }
    return 0
}