export { main }

import errors
import io
import net

func close_ignored(file: io.File) {
    _ = io.close(file)
}

func loopback(port: u16) net.IpEndpoint {
    return net.IpEndpoint{ addr: net.Ipv4Addr{ a: 127, b: 0, c: 0, d: 1 }, port: port }
}

func main(args: Slice<cstr>) i32 {
    if args.len < 2 {
        return 30
    }

    port: u16
    err: errors.Error
    port, err = net.parse_port(args[1])
    if !errors.is_ok(err) {
        return 31
    }

    conn: io.File
    conn, err = net.connect_tcp(loopback(port))
    if !errors.is_ok(err) {
        return 32
    }
    defer close_ignored(conn)

    poller: *io.Poller
    poller, err = io.poller_new()
    if !errors.is_ok(err) {
        return 33
    }
    defer io.poller_close(poller)

    err = io.poller_add(poller, conn, io.EVENT_WRITABLE)
    if !errors.is_ok(err) {
        return 34
    }

    events: [4]io.Event
    sent: bool = false

    while true {
        ready: usize
        ready, err = io.poller_wait(poller, (Slice<io.Event>)(events), 2000)
        if !errors.is_ok(err) {
            return 35
        }
        if ready == 0 {
            return 36
        }

        index: usize = 0
        while index < ready {
            event: io.Event = events[index]
            if event.failed {
                return 37
            }

            if !sent {
                if !event.writable {
                    index = index + 1
                    continue
                }

                request: Slice<u8> = Slice<u8>{ ptr: "ping".ptr, len: 4 }
                nwritten: usize
                nwritten, err = io.write_file(conn, request)
                if !errors.is_ok(err) {
                    return 38
                }
                if nwritten != 4 {
                    return 39
                }

                err = io.poller_set(poller, conn, io.EVENT_READABLE)
                if !errors.is_ok(err) {
                    return 40
                }
                sent = true
                index = index + 1
                continue
            }

            if !event.readable {
                index = index + 1
                continue
            }

            reply_buf: [8]u8
            reply: Slice<u8> = (Slice<u8>)(reply_buf)
            nread: usize
            nread, err = io.read(conn, reply)
            if !errors.is_ok(err) {
                return 41
            }
            if nread != 4 {
                return 42
            }
            if reply[0] != 112 {
                return 43
            }
            if reply[1] != 111 {
                return 44
            }
            if reply[2] != 110 {
                return 45
            }
            if reply[3] != 103 {
                return 46
            }
            return 0
        }
    }

    return 47
}