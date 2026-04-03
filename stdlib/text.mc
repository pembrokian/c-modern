export { eq, find_byte, trim_space, is_blank }

import strings

func eq(left: str, right: str) bool {
    return strings.eq(left, right)
}

func find_byte(text: str, needle: u8) usize {
    return strings.find_byte(text, needle)
}

func trim_space(text: str) str {
    return strings.trim_space(text)
}

func is_blank(text: str) bool {
    return strings.is_blank(text)
}