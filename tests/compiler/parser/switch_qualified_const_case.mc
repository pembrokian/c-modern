func choose(value: u8) i32 {
    switch value {
        case serial_protocol.CMD_E:
            return 10
        case serial_protocol.CMD_L:
            return 20
        default:
            return 30
    }
}