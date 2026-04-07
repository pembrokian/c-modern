@abi(c)
struct TextBox {
    text: cstr
}

extern(c) func strlen(text: cstr) usize

func measured_len(box: TextBox) usize {
    return strlen(box.text)
}

func main(args: Slice<cstr>) i32 {
    if args.len < 2 {
        return 90
    }

    box: TextBox = TextBox{ text: args[1] }
    if measured_len(box) != 6 {
        return 91
    }
    return 0
}