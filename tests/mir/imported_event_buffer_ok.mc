import helper_events

func main() i32 {
    events: [1]helper_events.Event
    events[0] = helper_events.Event{ file: 7, readable: true, writable: false, failed: false }
    first: helper_events.File = helper_events.first_file((Slice<helper_events.Event>)(events))
    return first
}
