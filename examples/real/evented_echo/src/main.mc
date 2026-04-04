export { main }

import errors
import echo_core
import io
import net

func main(args: Slice<cstr>) i32 {
    if args.len != 2 {
        status: i32 = io.write_line("usage: evented-echo <port>")
        if status != 0 {
            return status
        }
        return 64
    }

    return run(args[1])
}

func close_ignored(file: i32) {
    ignored: errors.Error = io.close(file)
    if !errors.is_ok(ignored) {
        return
    }
}

func poller_remove_ignored(poller: *io.Poller, file: i32) {
    ignored: errors.Error = io.poller_remove(poller, file)
    if !errors.is_ok(ignored) {
        return
    }
}

func run(port_text: str) i32 {
    port: u16
    parse_err: uintptr
    port, parse_err = echo_core.parse_port(port_text)
    if parse_err != 0 {
        return 10
    }

    bind: net.IpEndpoint = net.IpEndpoint{ addr: net.IpAddr{ a: 127, b: 0, c: 0, d: 1 }, port: port }
    listener: i32
    err: errors.Error
    listener, err = net.tcp_listen(bind)
    if !errors.is_ok(err) {
        return 11
    }
    defer close_ignored(listener)

    poller: *io.Poller
    poller, err = io.poller_new()
    if !errors.is_ok(err) {
        return 12
    }
    defer io.poller_close(poller)

    err = io.poller_add(poller, listener, io.EVENT_READABLE)
    if !errors.is_ok(err) {
        return 13
    }

    files: [4]i32
    readable: [4]u8
    writable: [4]u8
    failed: [4]u8
    conn: i32 = 0
    accepted: bool = false

    while true {
        ready: usize
        files_ptr: *i32 = (*i32)((uintptr)(&files))
        readable_ptr: *u8 = (*u8)((uintptr)(&readable))
        writable_ptr: *u8 = (*u8)((uintptr)(&writable))
        failed_ptr: *u8 = (*u8)((uintptr)(&failed))
        ready, err = io.poller_wait(poller, files_ptr, readable_ptr, writable_ptr, failed_ptr, 4, 2000)
        if !errors.is_ok(err) {
            if accepted {
                close_ignored(conn)
            }
            return 14
        }
        if ready == 0 {
            if accepted {
                close_ignored(conn)
            }
            return 15
        }

        index: usize = 0
        while index < ready {
            if failed[index] != 0 {
                if accepted {
                    close_ignored(conn)
                }
                return 16
            }

            if files[index] == listener {
                if readable[index] == 0 {
                    if accepted {
                        close_ignored(conn)
                    }
                    return 17
                }

                conn, err = net.accept(listener)
                if !errors.is_ok(err) {
                    return 18
                }
                accepted = true

                err = io.poller_add(poller, conn, io.EVENT_READABLE)
                if !errors.is_ok(err) {
                    close_ignored(conn)
                    return 19
                }

                err = io.poller_set(poller, conn, io.EVENT_READABLE)
                if !errors.is_ok(err) {
                    poller_remove_ignored(poller, conn)
                    close_ignored(conn)
                    return 20
                }

                index = index + 1
                continue
            }

            if !accepted {
                return 21
            }
            if files[index] != conn {
                close_ignored(conn)
                return 22
            }
            if readable[index] == 0 {
                close_ignored(conn)
                return 23
            }

            buf: [16]u8
            bytes: Slice<u8> = (Slice<u8>)(buf)
            nread: usize
            nread, err = io.read(conn, bytes)
            if !errors.is_ok(err) {
                poller_remove_ignored(poller, conn)
                close_ignored(conn)
                return 24
            }
            if nread == 0 {
                poller_remove_ignored(poller, conn)
                close_ignored(conn)
                return 25
            }

            nwritten: usize
            nwritten, err = io.write_file(conn, bytes[0:nread])
            if !errors.is_ok(err) {
                poller_remove_ignored(poller, conn)
                close_ignored(conn)
                return 26
            }
            if nwritten != nread {
                poller_remove_ignored(poller, conn)
                close_ignored(conn)
                return 27
            }

            poller_remove_ignored(poller, conn)
            close_ignored(conn)
            return 0
        }
    }

    return 28
}