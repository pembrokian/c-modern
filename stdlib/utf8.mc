import errors
import strings

struct Decoded {
    codepoint: u32
    width: usize
}

struct Encoded {
    bytes: [4]u8
    len: usize
}

const UTF8_ERR_INVALID_BYTES: usize = 1
const UTF8_ERR_INVALID_CODEPOINT: usize = 2
const UTF8_ASCII_HI: u8 = 128
const UTF8_CONT_LO: u8 = 128
const UTF8_CONT_HI: u8 = 192
const UTF8_LEAD_2_MIN: u8 = 194
const UTF8_LEAD_2_BASE: u8 = 192
const UTF8_LEAD_3_MIN: u8 = 224
const UTF8_LEAD_4_MIN: u8 = 240
const UTF8_LEAD_4_LIMIT: u8 = 245
const UTF8_LEAD_E0: u8 = 224
const UTF8_LEAD_ED: u8 = 237
const UTF8_LEAD_F0: u8 = 240
const UTF8_LEAD_F4: u8 = 244
const UTF8_SECOND_E0_MIN: u8 = 160
const UTF8_SECOND_F0_MIN: u8 = 144

func invalid_utf8() errors.Error {
    return errors.fail_utf8(UTF8_ERR_INVALID_BYTES)
}

func invalid_codepoint() errors.Error {
    return errors.fail_utf8(UTF8_ERR_INVALID_CODEPOINT)
}

func zero_utf8_bytes() [4]u8 {
    bytes: [4]u8
    bytes[0] = 0
    bytes[1] = 0
    bytes[2] = 0
    bytes[3] = 0
    return bytes
}

func empty_encoded() Encoded {
    return Encoded{ bytes: zero_utf8_bytes(), len: 0 }
}

func leading_codepoint_len_bytes(bytes: Slice<u8>) usize {
    if bytes.len == 0 {
        return 0
    }

    first: u8 = bytes[0]
    if first < UTF8_ASCII_HI {
        return 1
    }

    if first < UTF8_LEAD_2_MIN {
        return 0
    }

    if first < UTF8_LEAD_3_MIN {
        if bytes.len < 2 {
            return 0
        }
        second_2: u8 = bytes[1]
        if second_2 < UTF8_CONT_LO {
            return 0
        }
        if second_2 >= UTF8_CONT_HI {
            return 0
        }
        return 2
    }

    if first < UTF8_LEAD_4_MIN {
        if bytes.len < 3 {
            return 0
        }
        second_3: u8 = bytes[1]
        third_3: u8 = bytes[2]
        if second_3 < UTF8_CONT_LO {
            return 0
        }
        if second_3 >= UTF8_CONT_HI {
            return 0
        }
        if third_3 < UTF8_CONT_LO {
            return 0
        }
        if third_3 >= UTF8_CONT_HI {
            return 0
        }
        if first == UTF8_LEAD_E0 {
            if second_3 < UTF8_SECOND_E0_MIN {
                return 0
            }
        }
        if first == UTF8_LEAD_ED {
            if second_3 >= UTF8_SECOND_E0_MIN {
                return 0
            }
        }
        return 3
    }

    if first < UTF8_LEAD_4_LIMIT {
        if bytes.len < 4 {
            return 0
        }
        second_4: u8 = bytes[1]
        third_4: u8 = bytes[2]
        fourth_4: u8 = bytes[3]
        if second_4 < UTF8_CONT_LO {
            return 0
        }
        if second_4 >= UTF8_CONT_HI {
            return 0
        }
        if third_4 < UTF8_CONT_LO {
            return 0
        }
        if third_4 >= UTF8_CONT_HI {
            return 0
        }
        if fourth_4 < UTF8_CONT_LO {
            return 0
        }
        if fourth_4 >= UTF8_CONT_HI {
            return 0
        }
        if first == UTF8_LEAD_F0 {
            if second_4 < UTF8_SECOND_F0_MIN {
                return 0
            }
        }
        if first == UTF8_LEAD_F4 {
            if second_4 >= UTF8_SECOND_F0_MIN {
                return 0
            }
        }
        return 4
    }

    return 0
}

func decode_bytes(bytes: Slice<u8>) (Decoded, errors.Error) {
    empty: Decoded = Decoded{ codepoint: 0, width: 0 }

    step: usize = leading_codepoint_len_bytes(bytes)
    if step == 0 {
        return empty, invalid_utf8()
    }

    first: u8 = bytes[0]
    if first < UTF8_ASCII_HI {
        return Decoded{ codepoint: (u32)(first), width: 1 }, errors.ok()
    }

    if step == 2 {
        second_2: u8 = bytes[1]
        codepoint_2: u32 = ((u32)(first) - (u32)(UTF8_LEAD_2_BASE)) * (u32)(64) + ((u32)(second_2) - (u32)(UTF8_CONT_LO))
        return Decoded{ codepoint: codepoint_2, width: 2 }, errors.ok()
    }

    if step == 3 {
        second_3: u8 = bytes[1]
        third_3: u8 = bytes[2]
        codepoint_3: u32 = ((u32)(first) - (u32)(UTF8_LEAD_3_MIN)) * (u32)(4096) + ((u32)(second_3) - (u32)(UTF8_CONT_LO)) * (u32)(64) + ((u32)(third_3) - (u32)(UTF8_CONT_LO))
        return Decoded{ codepoint: codepoint_3, width: 3 }, errors.ok()
    }

    second_4: u8 = bytes[1]
    third_4: u8 = bytes[2]
    fourth_4: u8 = bytes[3]
    codepoint_4: u32 = ((u32)(first) - (u32)(UTF8_LEAD_4_MIN)) * (u32)(262144) + ((u32)(second_4) - (u32)(UTF8_CONT_LO)) * (u32)(4096) + ((u32)(third_4) - (u32)(UTF8_CONT_LO)) * (u32)(64) + ((u32)(fourth_4) - (u32)(UTF8_CONT_LO))
    return Decoded{ codepoint: codepoint_4, width: 4 }, errors.ok()
}

func encode_into(codepoint: u32) (Encoded, errors.Error) {
    empty: Encoded = empty_encoded()
    bytes: [4]u8 = zero_utf8_bytes()
    if codepoint <= 127 {
        bytes[0] = (u8)(codepoint)
        return Encoded{ bytes: bytes, len: 1 }, errors.ok()
    }
    if codepoint <= 2047 {
        bytes[0] = (u8)(192 + codepoint / 64)
        bytes[1] = (u8)(128 + codepoint % 64)
        return Encoded{ bytes: bytes, len: 2 }, errors.ok()
    }
    if codepoint >= 55296 {
        if codepoint <= 57343 {
            return empty, invalid_codepoint()
        }
    }
    if codepoint <= 65535 {
        bytes[0] = (u8)(224 + codepoint / 4096)
        bytes[1] = (u8)(128 + (codepoint / 64) % 64)
        bytes[2] = (u8)(128 + codepoint % 64)
        return Encoded{ bytes: bytes, len: 3 }, errors.ok()
    }
    if codepoint > 1114111 {
        return empty, invalid_codepoint()
    }
    bytes[0] = (u8)(240 + codepoint / 262144)
    bytes[1] = (u8)(128 + (codepoint / 4096) % 64)
    bytes[2] = (u8)(128 + (codepoint / 64) % 64)
    bytes[3] = (u8)(128 + codepoint % 64)
    return Encoded{ bytes: bytes, len: 4 }, errors.ok()
}

func valid_bytes(bytes: Slice<u8>) bool {
    index: usize = 0
    while index < bytes.len {
        step: usize = leading_codepoint_len_bytes(bytes[index:bytes.len])
        if step == 0 {
            return false
        }
        index = index + step
    }

    return true
}

func byte_len(text: str) usize {
    return strings.byte_len(text)
}

func empty(text: str) bool {
    return strings.empty(text)
}

func valid(text: str) bool {
    return valid_bytes(strings.bytes(text))
}

func leading_codepoint_width(text: str) usize {
    return leading_codepoint_len_bytes(strings.bytes(text))
}

func decode(text: str) (Decoded, errors.Error) {
    return decode_bytes(strings.bytes(text))
}

func encode(codepoint: u32) (Encoded, errors.Error) {
    return encode_into(codepoint)
}