enum Choice {
    Small(tag: i8, value: f64),
    Count(count: i32),
}

func payload_roundtrip() f64 {
    value: Choice = Choice.Small(7, 4.25)
    switch value {
    case Choice.Small(tag, payload):
        return payload
    default:
        return 0.0
    }
}