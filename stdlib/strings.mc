@private
extern(c) func __mc_strings_eq(left: str, right: str) i32
@private
extern(c) func __mc_strings_find_byte(text: str, needle: u8) usize
@private
extern(c) func __mc_strings_trim_space_start(text: str) usize
@private
extern(c) func __mc_strings_trim_space_end(text: str) usize

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
    // Returns text.len when needle is absent.
    return __mc_strings_find_byte(text, needle)
}

func trim_space(text: str) str {
    // Space is the ASCII subset: space, tab, newline, and carriage return.
    start: usize = __mc_strings_trim_space_start(text)
    content_end: usize = __mc_strings_trim_space_end(text)
    if content_end < start {
        return text[0:0]
    }
    return text[start:content_end]
}

func is_blank(text: str) bool {
    return empty(trim_space(text))
}
