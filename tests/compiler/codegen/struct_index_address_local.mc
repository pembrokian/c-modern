struct Endpoint {
    pid: i32,
    active: bool,
}

func replace(ptr: *Endpoint, pid: i32, active: bool) {
    *ptr = Endpoint{ pid: pid, active: active }
}

func main() i32 {
    endpoints: [2]Endpoint
    endpoints[0] = Endpoint{ pid: 4, active: true }
    endpoints[1] = Endpoint{ pid: 9, active: false }

    first: *Endpoint = &endpoints[0]
    second: *Endpoint = &((endpoints)[1])
    replace(first, 5, false)
    replace(second, 10, true)

    if endpoints[0].pid != 5 {
        return 10
    }
    if endpoints[1].pid != 10 {
        return 11
    }
    if endpoints[0].active {
        return 12
    }
    if !endpoints[1].active {
        return 13
    }
    if first == second {
        return 14
    }
    return 0
}