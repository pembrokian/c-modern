struct Endpoint {
    pid: u32,
    active: bool,
}

func active_count(endpoints: [2]Endpoint) usize {
    view: Slice<Endpoint> = (Slice<Endpoint>)(endpoints)
    if view[0].active {
        return view.len
    }
    return 0
}

func main() usize {
    endpoints: [2]Endpoint
    endpoints[0] = Endpoint{ pid: 7, active: true }
    endpoints[1] = Endpoint{ pid: 9, active: false }
    return active_count(endpoints)
}