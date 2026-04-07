import errors
import io
import strings

struct Ipv4Addr {
    a: u8
    b: u8
    c: u8
    d: u8
}

struct IpEndpoint {
    addr: Ipv4Addr
    port: u16
}

const NET_ERR_INVALID_PORT: usize = 1
const ASCII_ZERO: u8 = 48
const ASCII_NINE: u8 = 57

extern(c) func __mc_net_tcp_listen(a: u8, b: u8, c: u8, d: u8, port: u16, err: *errors.Error) i32
extern(c) func __mc_net_accept(listener: io.File, err: *errors.Error) i32
extern(c) func __mc_net_connect_tcp(a: u8, b: u8, c: u8, d: u8, port: u16, err: *errors.Error) i32

func parse_port(text: str) (u16, errors.Error) {
    if text.len == 0 {
        return 0, errors.fail_net(NET_ERR_INVALID_PORT)
    }

    bytes: Slice<u8> = strings.bytes(text)
    value: u32 = 0
    index: usize = 0
    while index < bytes.len {
        ch: u8 = bytes[index]
        if ch < ASCII_ZERO {
            return 0, errors.fail_net(NET_ERR_INVALID_PORT)
        }
        if ch > ASCII_NINE {
            return 0, errors.fail_net(NET_ERR_INVALID_PORT)
        }
        value = value * (u32)(10) + (u32)(ch - ASCII_ZERO)
        if value > (u32)(65535) {
            return 0, errors.fail_net(NET_ERR_INVALID_PORT)
        }
        index = index + 1
    }

    return (u16)(value), errors.ok()
}

func tcp_listen(bind: IpEndpoint) (io.File, errors.Error) {
    err: errors.Error = errors.ok()
    file: io.File = __mc_net_tcp_listen(bind.addr.a, bind.addr.b, bind.addr.c, bind.addr.d, bind.port, &err)
    return file, err
}

func accept(listener: io.File) (io.File, errors.Error) {
    err: errors.Error = errors.ok()
    file: io.File = __mc_net_accept(listener, &err)
    return file, err
}

func connect_tcp(remote: IpEndpoint) (io.File, errors.Error) {
    err: errors.Error = errors.ok()
    file: io.File = __mc_net_connect_tcp(remote.addr.a, remote.addr.b, remote.addr.c, remote.addr.d, remote.port, &err)
    return file, err
}