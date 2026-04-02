export { eq, find_byte, trim_space, is_blank }

import strings

extern(c) func __mc_strings_eq(left: str, right: str) bool
extern(c) func __mc_strings_find_byte(text: str, needle: u8) usize
extern(c) func __mc_strings_trim_space_start(text: str) usize
extern(c) func __mc_strings_trim_space_end(text: str) usize

func eq(left: str, right: str) bool {
    return __mc_strings_eq(left, right)
}

func find_byte(text: str, needle: u8) usize {
    return __mc_strings_find_byte(text, needle)
}

func trim_space(text: str) str {
    start: usize = __mc_strings_trim_space_start(text)
    end: usize = __mc_strings_trim_space_end(text)
    return text[start:end]
}

func is_blank(text: str) bool {
    return strings.empty(trim_space(text))
}