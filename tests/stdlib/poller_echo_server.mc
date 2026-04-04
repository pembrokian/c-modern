export { main }

import errors
import io
import net

func close_ignored(file: io.File) {
    _ = io.close(file)
}

func poller_remove_ignored(poller: *io.Poller, file: io.File) {
    ignored: errors.Error = io.poller_remove(poller, file)
    if !errors.is_ok(ignored) {
        return
    }
}

func parse_port(text: str) (u16, errors.Error) {
    if text.len == 0 {
        return 0, errors.fail_code(1)
    }

    bytes: Slice<u8> = Slice<u8>{ ptr: text.ptr, len: text.len }
    value: u32 = 0
    index: usize = 0
    while index < bytes.len {
        ch: u8 = bytes[index]
        if ch < 48 {
            return 0, errors.fail_code(1)
        }
        if ch > 57 {
            return 0, errors.fail_code(1)
        }
        value = value * (u32)(10) + (u32)(ch - 48)
        if value > (u32)(65535) {
            return 0, errors.fail_code(1)
        }
        index = index + 1
    }

    return (u16)(value), errors.ok()
}

func loopback(port: u16) net.IpEndpoint {
    return net.IpEndpoint{ addr: net.IpAddr{ a: 127, b: 0, c: 0, d: 1 }, port: port }
}

func main(args: Slice<cstr>) i32 {
    if args.len < 2 {
        return 10
    }

    port: u16
    err: errors.Error
    port, err = parse_port(args[1])
    if !errors.is_ok(err) {
        return 11
    }

    listener: io.File
    listener, err = net.tcp_listen(loopback(port))
    if !errors.is_ok(err) {
        return 12
    }
    defer close_ignored(listener)

    poller: *io.Poller
    poller, err = io.poller_new()
    if !errors.is_ok(err) {
        return 13
    }
    defer io.poller_close(poller)

    err = io.poller_add(poller, listener, io.EVENT_READABLE)
    if !errors.is_ok(err) {
        return 14
    }

    events: [4]io.Event
    conn: io.File = 0
    accepted: bool = false

    while true {
        ready: usize
        ready, err = io.poller_wait(poller, (Slice<io.Event>)(events), 2000)
        if !errors.is_ok(err) {
            if accepted {
                close_ignored(conn)
            }
            return 15
        }
        if ready == 0 {
            if accepted {
                close_ignored(conn)
            }
            return 16
        }

        index: usize = 0
        while index < ready {
            event: io.Event = events[index]
            if event.failed != 0 {
                if accepted {
                    close_ignored(conn)
                }
                return 17
            }

            if event.file == listener {
                if event.readable == 0 {
                    if accepted {
                        close_ignored(conn)
                    }
                    return 18
                }

                conn, err = net.accept(listener)
                if !errors.is_ok(err) {
                    return 19
                }
                accepted = true

                err = io.poller_add(poller, conn, io.EVENT_READABLE)
                if !errors.is_ok(err) {
                    close_ignored(conn)
                    return 20
                }

                err = io.poller_set(poller, conn, io.EVENT_READABLE)
                if !errors.is_ok(err) {
                    poller_remove_ignored(poller, conn)
                    close_ignored(conn)
                    return 21
                }

                index = index + 1
                continue
            }

            if !accepted {
                return 22
            }
            if event.file != conn {
                close_ignored(conn)
                return 23
            }
            if event.readable == 0 {
                close_ignored(conn)
                return 24
            }

            buf: [16]u8
            bytes: Slice<u8> = (Slice<u8>)(buf)
            nread: usize
            nread, err = io.read(conn, bytes)
            if !errors.is_ok(err) {
                poller_remove_ignored(poller, conn)
                close_ignored(conn)
                return 25
            }
            if nread == 0 {
                poller_remove_ignored(poller, conn)
                close_ignored(conn)
                return 26
            }

            nwritten: usize
            nwritten, err = io.write_file(conn, bytes[0:nread])
            if !errors.is_ok(err) {
                poller_remove_ignored(poller, conn)
                close_ignored(conn)
                return 27
            }
            if nwritten != nread {
                poller_remove_ignored(poller, conn)
                close_ignored(conn)
                return 28
            }

            poller_remove_ignored(poller, conn)
            close_ignored(conn)
            return 0
        }
    }

    return 29
}