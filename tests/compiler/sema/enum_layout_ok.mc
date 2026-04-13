enum MessageTag {
    Empty,
    Ping,
    Ack,
}

struct Envelope {
    tag: MessageTag
    sender: u32
}

func main() i32 {
    message: Envelope = Envelope{ tag: MessageTag.Ping, sender: 7 }
    if message.sender != 7 {
        return 1
    }
    return 0
}