export { IpAddr, IpEndpoint, tcp_listen, accept, connect_tcp }

import errors

struct IpAddr {
    a: u8
    b: u8
    c: u8
    d: u8
}

struct IpEndpoint {
    addr: IpAddr
    port: u16
}

extern(c) func __mc_net_tcp_listen(a: u8, b: u8, c: u8, d: u8, port: u16, err: *errors.Error) i32
extern(c) func __mc_net_accept(listener: i32, err: *errors.Error) i32
extern(c) func __mc_net_connect_tcp(a: u8, b: u8, c: u8, d: u8, port: u16, err: *errors.Error) i32

func tcp_listen(bind: IpEndpoint) (i32, errors.Error) {
    err: errors.Error = errors.ok()
    file: i32 = __mc_net_tcp_listen(bind.addr.a, bind.addr.b, bind.addr.c, bind.addr.d, bind.port, &err)
    return file, err
}

func accept(listener: i32) (i32, errors.Error) {
    err: errors.Error = errors.ok()
    file: i32 = __mc_net_accept(listener, &err)
    return file, err
}

func connect_tcp(remote: IpEndpoint) (i32, errors.Error) {
    err: errors.Error = errors.ok()
    file: i32 = __mc_net_connect_tcp(remote.addr.a, remote.addr.b, remote.addr.c, remote.addr.d, remote.port, &err)
    return file, err
}