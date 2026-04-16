import switch_consts

func choose(value: u8) i32 {
    switch value {
        case switch_consts.CMD_A:
            return 10
        case switch_consts.CMD_B:
            return 20
        default:
            return 30
    }
}