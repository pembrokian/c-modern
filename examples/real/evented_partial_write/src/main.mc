import errors
import io
import net
import partial_write_core

const REQUEST_LEN: usize = 4
const RESPONSE_LEN: usize = 1536
const STATE_READ_REQUEST: i32 = 0
const STATE_WRITE_RESPONSE: i32 = 1
const STATE_READ_ACK: i32 = 2

func close_ignored(file: io.File) {
    if errors.is_err(io.close(file)) {
    }
}

func poller_remove_ignored(poller: *io.Poller, file: io.File) {
    ignored: errors.Error = io.poller_remove(poller, file)
    if !errors.is_ok(ignored) {
        return
    }
}

func loopback(port: u16) net.IpEndpoint {
    return net.IpEndpoint{ addr: net.Ipv4Addr{ a: 127, b: 0, c: 0, d: 1 }, port: port }
}

func main(args: Slice<cstr>) i32 {
    if args.len != 2 {
        if io.write_line("usage: evented-partial-write <port>") != 0 {
            return 1
        }
        return 80
    }

    port: u16
    parse_err: errors.Error
    port, parse_err = net.parse_port(args[1])
    if errors.is_err(parse_err) {
        return 81
    }

    listener: io.File
    err: errors.Error
    listener, err = net.tcp_listen(loopback(port))
    if !errors.is_ok(err) {
        return 82
    }
    defer close_ignored(listener)

    poller: *io.Poller
    poller, err = io.poller_new()
    if !errors.is_ok(err) {
        return 83
    }
    defer io.poller_close(poller)

    err = io.poller_add(poller, listener, io.EVENT_READABLE)
    if !errors.is_ok(err) {
        return 84
    }

    response_buf: [1536]u8
    response: Slice<u8> = (Slice<u8>)(response_buf)
    partial_write_core.fill_response(response)

    events: [4]io.Event
    conn: io.File = 0
    accepted: bool = false
    conn_state: i32 = STATE_READ_REQUEST
    written: usize = 0

    while true {
        ready: usize
        ready, err = io.poller_wait(poller, (Slice<io.Event>)(events), 2000)
        if !errors.is_ok(err) {
            if accepted {
                close_ignored(conn)
            }
            return 85
        }
        if ready == 0 {
            if accepted {
                close_ignored(conn)
            }
            return 86
        }

        index: usize = 0
        while index < ready {
            event: io.Event = events[index]
            if event.failed {
                if accepted {
                    close_ignored(conn)
                }
                return 87
            }

            if event.file == listener {
                if !event.readable {
                    if accepted {
                        close_ignored(conn)
                    }
                    return 88
                }

                conn, err = net.accept(listener)
                if !errors.is_ok(err) {
                    return 89
                }
                accepted = true

                err = io.poller_add(poller, conn, io.EVENT_READABLE)
                if !errors.is_ok(err) {
                    close_ignored(conn)
                    return 90
                }

                err = io.poller_set(poller, conn, io.EVENT_READABLE)
                if !errors.is_ok(err) {
                    poller_remove_ignored(poller, conn)
                    close_ignored(conn)
                    return 91
                }

                index = index + 1
                continue
            }

            if !accepted {
                return 92
            }
            if event.file != conn {
                close_ignored(conn)
                return 93
            }

            if conn_state == STATE_READ_REQUEST {
                if !event.readable {
                    index = index + 1
                    continue
                }

                request_buf: [8]u8
                request: Slice<u8> = (Slice<u8>)(request_buf)
                nread: usize
                nread, err = io.read(conn, request)
                if !errors.is_ok(err) {
                    poller_remove_ignored(poller, conn)
                    close_ignored(conn)
                    return 94
                }
                if nread != REQUEST_LEN {
                    poller_remove_ignored(poller, conn)
                    close_ignored(conn)
                    return 95
                }
                if request[0] != 112 {
                    poller_remove_ignored(poller, conn)
                    close_ignored(conn)
                    return 96
                }
                if request[1] != 117 {
                    poller_remove_ignored(poller, conn)
                    close_ignored(conn)
                    return 96
                }
                if request[2] != 115 {
                    poller_remove_ignored(poller, conn)
                    close_ignored(conn)
                    return 96
                }
                if request[3] != 104 {
                    poller_remove_ignored(poller, conn)
                    close_ignored(conn)
                    return 96
                }

                conn_state = STATE_WRITE_RESPONSE
                written = 0
                err = io.poller_set(poller, conn, io.EVENT_WRITABLE)
                if !errors.is_ok(err) {
                    poller_remove_ignored(poller, conn)
                    close_ignored(conn)
                    return 97
                }

                index = index + 1
                continue
            }

            if conn_state == STATE_WRITE_RESPONSE {
                if !event.writable {
                    index = index + 1
                    continue
                }

                remaining: Slice<u8> = response[written:response.len]
                chunk_len: usize = partial_write_core.write_chunk_len(remaining.len)
                if chunk_len < remaining.len {
                    remaining = remaining[0:chunk_len]
                }

                nwritten: usize
                nwritten, err = io.write_file(conn, remaining)
                if !errors.is_ok(err) {
                    poller_remove_ignored(poller, conn)
                    close_ignored(conn)
                    return 98
                }
                if nwritten == 0 {
                    poller_remove_ignored(poller, conn)
                    close_ignored(conn)
                    return 99
                }

                written = written + nwritten
                if written == response.len {
                    conn_state = STATE_READ_ACK
                    err = io.poller_set(poller, conn, io.EVENT_READABLE)
                    if !errors.is_ok(err) {
                        poller_remove_ignored(poller, conn)
                        close_ignored(conn)
                        return 100
                    }
                }

                index = index + 1
                continue
            }

            if !event.readable {
                index = index + 1
                continue
            }

            ack_buf: [2]u8
            ack: Slice<u8> = (Slice<u8>)(ack_buf)
            nack: usize
            nack, err = io.read(conn, ack[0:1])
            if !errors.is_ok(err) {
                poller_remove_ignored(poller, conn)
                close_ignored(conn)
                return 101
            }
            if nack != 1 {
                poller_remove_ignored(poller, conn)
                close_ignored(conn)
                return 102
            }
            if ack[0] != 33 {
                poller_remove_ignored(poller, conn)
                close_ignored(conn)
                return 102
            }

            poller_remove_ignored(poller, conn)
            close_ignored(conn)
            return 0
        }
    }

    return 103
}