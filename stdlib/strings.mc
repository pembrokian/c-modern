export { byte_len, bytes, empty, eq, find_byte, trim_space, is_blank }

extern(c) func __mc_strings_eq(left: str, right: str) i32
extern(c) func __mc_strings_find_byte(text: str, needle: u8) usize
extern(c) func __mc_strings_trim_space_start(text: str) usize
extern(c) func __mc_strings_trim_space_end(text: str) usize

func byte_len(text: str) usize {
    return text.len
}

func bytes(text: str) Slice<u8> {
    return Slice<u8>{ ptr: text.ptr, len: text.len }
}

func empty(text: str) bool {
    return text.len == 0
}

func eq(left: str, right: str) bool {
    return __mc_strings_eq(left, right) != 0
}

func find_byte(text: str, needle: u8) usize {
    return __mc_strings_find_byte(text, needle)
}

func trim_space(text: str) str {
    start: usize = __mc_strings_trim_space_start(text)
    end: usize = __mc_strings_trim_space_end(text)
    if end < start {
        return text[0:0]
    }
    return text[start:end]
}

func is_blank(text: str) bool {
    return empty(trim_space(text))
}