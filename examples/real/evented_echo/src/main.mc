export { main }

import echo_core

struct Poller {
    raw: uintptr
}

const EVENT_READABLE: u32 = 1

extern(c) func __mc_io_write_line(text: str) i32
extern(c) func __mc_io_read(file: i32, bytes: Slice<u8>, err: *uintptr) usize
extern(c) func __mc_io_write_file(file: i32, bytes: Slice<u8>, err: *uintptr) usize
extern(c) func __mc_io_close(file: i32) uintptr
extern(c) func __mc_io_poller_new(err: *uintptr) *Poller
extern(c) func __mc_io_poller_add(p: *Poller, file: i32, interests: u32) uintptr
extern(c) func __mc_io_poller_set(p: *Poller, file: i32, interests: u32) uintptr
extern(c) func __mc_io_poller_remove(p: *Poller, file: i32) uintptr
extern(c) func __mc_io_poller_wait(p: *Poller, files: *i32, readable: *u8, writable: *u8, failed: *u8, cap: usize, timeout_ms: i32, err: *uintptr) usize
extern(c) func __mc_io_poller_close(p: *Poller)
extern(c) func __mc_net_tcp_listen(a: u8, b: u8, c: u8, d: u8, port: u16, err: *uintptr) i32
extern(c) func __mc_net_accept(listener: i32, err: *uintptr) i32

func main(args: Slice<cstr>) i32 {
    if args.len != 2 {
        status: i32 = __mc_io_write_line("usage: evented-echo <port>")
        if status != 0 {
            return status
        }
        return 64
    }

    return run(args[1])
}

func close_ignored(file: i32) {
    ignored = __mc_io_close(file)
    if ignored != 0 {
        return
    }
}

func poller_remove_ignored(poller: *Poller, file: i32) {
    ignored = __mc_io_poller_remove(poller, file)
    if ignored != 0 {
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

    listener: i32
    listener_err: uintptr = 0
    listener = __mc_net_tcp_listen(127, 0, 0, 1, port, &listener_err)
    if listener_err != 0 {
        return 11
    }
    defer close_ignored(listener)

    poller: *Poller
    poller_err: uintptr = 0
    poller = __mc_io_poller_new(&poller_err)
    if poller_err != 0 {
        return 12
    }
    defer __mc_io_poller_close(poller)

    add_listener_err: uintptr = __mc_io_poller_add(poller, listener, EVENT_READABLE)
    if add_listener_err != 0 {
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
        wait_err: uintptr = 0
        ready = __mc_io_poller_wait(poller, files_ptr, readable_ptr, writable_ptr, failed_ptr, 4, 2000, &wait_err)
        if wait_err != 0 {
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

                accept_err: uintptr = 0
                conn = __mc_net_accept(listener, &accept_err)
                if accept_err != 0 {
                    return 18
                }
                accepted = true

                add_conn_err: uintptr = __mc_io_poller_add(poller, conn, EVENT_READABLE)
                if add_conn_err != 0 {
                    close_ignored(conn)
                    return 19
                }

                set_conn_err: uintptr = __mc_io_poller_set(poller, conn, EVENT_READABLE)
                if set_conn_err != 0 {
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
            read_err: uintptr = 0
            nread = __mc_io_read(conn, bytes, &read_err)
            if read_err != 0 {
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
            write_err: uintptr = 0
            nwritten = __mc_io_write_file(conn, bytes[0:nread], &write_err)
            if write_err != 0 {
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