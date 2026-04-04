export { main }

import errors
import io
import net
import strings

const REQUEST_LEN: usize = 4
const RESPONSE_LEN: usize = 1536
const WRITE_LIMIT: usize = 128
const STATE_READ_REQUEST: i32 = 0
const STATE_WRITE_RESPONSE: i32 = 1
const STATE_READ_ACK: i32 = 2

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

    bytes: Slice<u8> = strings.bytes(text)
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

func fill_response(bytes: Slice<u8>) {
    pattern: Slice<u8> = strings.bytes("phase16-partial-write-state|")
    index: usize = 0
    while index < bytes.len {
        bytes[index] = pattern[index % pattern.len]
        index = index + 1
    }
}

func main(args: Slice<cstr>) i32 {
    if args.len < 2 {
        return 50
    }

    port: u16
    err: errors.Error
    port, err = parse_port(args[1])
    if !errors.is_ok(err) {
        return 51
    }

    listener: io.File
    listener, err = net.tcp_listen(loopback(port))
    if !errors.is_ok(err) {
        return 52
    }
    defer close_ignored(listener)

    poller: *io.Poller
    poller, err = io.poller_new()
    if !errors.is_ok(err) {
        return 53
    }
    defer io.poller_close(poller)

    err = io.poller_add(poller, listener, io.EVENT_READABLE)
    if !errors.is_ok(err) {
        return 54
    }

    response_buf: [1536]u8
    response: Slice<u8> = (Slice<u8>)(response_buf)
    fill_response(response)

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
            return 55
        }
        if ready == 0 {
            if accepted {
                close_ignored(conn)
            }
            return 56
        }

        index: usize = 0
        while index < ready {
            event: io.Event = events[index]
            if event.failed {
                if accepted {
                    close_ignored(conn)
                }
                return 57
            }

            if event.file == listener {
                if !event.readable {
                    if accepted {
                        close_ignored(conn)
                    }
                    return 58
                }

                conn, err = net.accept(listener)
                if !errors.is_ok(err) {
                    return 59
                }
                accepted = true

                err = io.poller_add(poller, conn, io.EVENT_READABLE)
                if !errors.is_ok(err) {
                    close_ignored(conn)
                    return 60
                }

                err = io.poller_set(poller, conn, io.EVENT_READABLE)
                if !errors.is_ok(err) {
                    poller_remove_ignored(poller, conn)
                    close_ignored(conn)
                    return 61
                }

                index = index + 1
                continue
            }

            if !accepted {
                return 62
            }
            if event.file != conn {
                close_ignored(conn)
                return 63
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
                    return 64
                }
                if nread != REQUEST_LEN {
                    poller_remove_ignored(poller, conn)
                    close_ignored(conn)
                    return 65
                }
                if request[0] != 112 {
                    poller_remove_ignored(poller, conn)
                    close_ignored(conn)
                    return 66
                }
                if request[1] != 117 {
                    poller_remove_ignored(poller, conn)
                    close_ignored(conn)
                    return 66
                }
                if request[2] != 115 {
                    poller_remove_ignored(poller, conn)
                    close_ignored(conn)
                    return 66
                }
                if request[3] != 104 {
                    poller_remove_ignored(poller, conn)
                    close_ignored(conn)
                    return 66
                }

                conn_state = STATE_WRITE_RESPONSE
                written = 0
                err = io.poller_set(poller, conn, io.EVENT_WRITABLE)
                if !errors.is_ok(err) {
                    poller_remove_ignored(poller, conn)
                    close_ignored(conn)
                    return 67
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
                if remaining.len > WRITE_LIMIT {
                    remaining = remaining[0:WRITE_LIMIT]
                }

                nwritten: usize
                nwritten, err = io.write_file(conn, remaining)
                if !errors.is_ok(err) {
                    poller_remove_ignored(poller, conn)
                    close_ignored(conn)
                    return 68
                }
                if nwritten == 0 {
                    poller_remove_ignored(poller, conn)
                    close_ignored(conn)
                    return 69
                }

                written = written + nwritten
                if written == response.len {
                    conn_state = STATE_READ_ACK
                    err = io.poller_set(poller, conn, io.EVENT_READABLE)
                    if !errors.is_ok(err) {
                        poller_remove_ignored(poller, conn)
                        close_ignored(conn)
                        return 70
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
                return 71
            }
            if nack != 1 {
                poller_remove_ignored(poller, conn)
                close_ignored(conn)
                return 72
            }
            if ack[0] != 33 {
                poller_remove_ignored(poller, conn)
                close_ignored(conn)
                return 72
            }

            poller_remove_ignored(poller, conn)
            close_ignored(conn)
            return 0
        }
    }

    return 73
}