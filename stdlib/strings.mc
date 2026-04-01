export { byte_len, empty }

func byte_len(text: str) usize {
    return text.len
}

func empty(text: str) bool {
    return text.len == 0
}