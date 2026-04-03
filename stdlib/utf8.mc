export { Decoded, Encoded, byte_len, empty, valid, valid_bytes, codepoint_len, decode, encode }

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

func invalid_utf8() errors.Error {
    return errors.fail_code(1)
}

func invalid_codepoint() errors.Error {
    return errors.fail_code(2)
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

func codepoint_len_bytes(bytes: Slice<u8>) usize {
    ascii_hi: u8 = 128
    cont_lo: u8 = 128
    cont_hi: u8 = 192
    lead_2_min: u8 = 194
    lead_3_min: u8 = 224
    lead_4_min: u8 = 240
    lead_4_limit: u8 = 245
    lead_e0: u8 = 224
    lead_ed: u8 = 237
    lead_f0: u8 = 240
    lead_f4: u8 = 244
    second_e0_min: u8 = 160
    second_f0_min: u8 = 144
    if bytes.len == 0 {
        return 0
    }

    first: u8 = bytes[0]
    if first < ascii_hi {
        return 1
    }

    if first < lead_2_min {
        return 0
    }

    if first < lead_3_min {
        if bytes.len < 2 {
            return 0
        }
        second_2: u8 = bytes[1]
        if second_2 < cont_lo {
            return 0
        }
        if second_2 >= cont_hi {
            return 0
        }
        return 2
    }

    if first < lead_4_min {
        if bytes.len < 3 {
            return 0
        }
        second_3: u8 = bytes[1]
        third_3: u8 = bytes[2]
        if second_3 < cont_lo {
            return 0
        }
        if second_3 >= cont_hi {
            return 0
        }
        if third_3 < cont_lo {
            return 0
        }
        if third_3 >= cont_hi {
            return 0
        }
        if first == lead_e0 {
            if second_3 < second_e0_min {
                return 0
            }
        }
        if first == lead_ed {
            if second_3 >= second_e0_min {
                return 0
            }
        }
        return 3
    }

    if first < lead_4_limit {
        if bytes.len < 4 {
            return 0
        }
        second_4: u8 = bytes[1]
        third_4: u8 = bytes[2]
        fourth_4: u8 = bytes[3]
        if second_4 < cont_lo {
            return 0
        }
        if second_4 >= cont_hi {
            return 0
        }
        if third_4 < cont_lo {
            return 0
        }
        if third_4 >= cont_hi {
            return 0
        }
        if fourth_4 < cont_lo {
            return 0
        }
        if fourth_4 >= cont_hi {
            return 0
        }
        if first == lead_f0 {
            if second_4 < second_f0_min {
                return 0
            }
        }
        if first == lead_f4 {
            if second_4 >= second_f0_min {
                return 0
            }
        }
        return 4
    }

    return 0
}

func decode_bytes(bytes: Slice<u8>) (Decoded, errors.Error) {
    ascii_hi: u8 = 128
    cont_lo: u8 = 128
    lead_3_min: u8 = 224
    lead_4_min: u8 = 240
    empty: Decoded = Decoded{ codepoint: 0, width: 0 }

    step: usize = codepoint_len_bytes(bytes)
    if step == 0 {
        return empty, invalid_utf8()
    }

    first: u8 = bytes[0]
    if first < ascii_hi {
        return Decoded{ codepoint: (u32)(first), width: 1 }, errors.ok()
    }

    if step == 2 {
        second_2: u8 = bytes[1]
        codepoint_2: u32 = ((u32)(first) - (u32)(192)) * (u32)(64) + ((u32)(second_2) - (u32)(cont_lo))
        return Decoded{ codepoint: codepoint_2, width: 2 }, errors.ok()
    }

    if step == 3 {
        second_3: u8 = bytes[1]
        third_3: u8 = bytes[2]
        codepoint_3: u32 = ((u32)(first) - (u32)(lead_3_min)) * (u32)(4096) + ((u32)(second_3) - (u32)(cont_lo)) * (u32)(64) + ((u32)(third_3) - (u32)(cont_lo))
        return Decoded{ codepoint: codepoint_3, width: 3 }, errors.ok()
    }

    second_4: u8 = bytes[1]
    third_4: u8 = bytes[2]
    fourth_4: u8 = bytes[3]
    codepoint_4: u32 = ((u32)(first) - (u32)(lead_4_min)) * (u32)(262144) + ((u32)(second_4) - (u32)(cont_lo)) * (u32)(4096) + ((u32)(third_4) - (u32)(cont_lo)) * (u32)(64) + ((u32)(fourth_4) - (u32)(cont_lo))
    return Decoded{ codepoint: codepoint_4, width: 4 }, errors.ok()
}

func encode_into(codepoint: u32) (Encoded, errors.Error) {
    empty: Encoded = empty_encoded()
    bytes = zero_utf8_bytes()
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
        step: usize = codepoint_len_bytes(bytes[index:bytes.len])
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

func valid(text: string) bool {
    bytes: Slice<u8> = Slice<u8>{ ptr: text.ptr, len: text.len }
    return valid_bytes(bytes)
}

func codepoint_len(text: str) usize {
    bytes: Slice<u8> = Slice<u8>{ ptr: text.ptr, len: text.len }
    return codepoint_len_bytes(bytes)
}

func decode(text: str) (Decoded, errors.Error) {
    bytes: Slice<u8> = Slice<u8>{ ptr: text.ptr, len: text.len }
    return decode_bytes(bytes)
}

func encode(codepoint: u32) (Encoded, errors.Error) {
    return encode_into(codepoint)
}