export { byte_len, empty, valid }

import strings

func valid_bytes(bytes: Slice<u8>) bool {
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
    index: usize = 0
    while index < bytes.len {
        first: u8 = bytes[index]
        if first < ascii_hi {
            index = index + 1
            continue
        }

        if first < lead_2_min {
            return false
        }

        if first < lead_3_min {
            if index + 1 >= bytes.len {
                return false
            }
            second_2: u8 = bytes[index + 1]
            if second_2 < cont_lo {
                return false
            }
            if second_2 >= cont_hi {
                return false
            }
            index = index + 2
            continue
        }

        if first < lead_4_min {
            if index + 2 >= bytes.len {
                return false
            }
            second_3: u8 = bytes[index + 1]
            third_3: u8 = bytes[index + 2]
            if second_3 < cont_lo {
                return false
            }
            if second_3 >= cont_hi {
                return false
            }
            if third_3 < cont_lo {
                return false
            }
            if third_3 >= cont_hi {
                return false
            }
            if first == lead_e0 {
                if second_3 < second_e0_min {
                    return false
                }
            }
            if first == lead_ed {
                if second_3 >= second_e0_min {
                    return false
                }
            }
            index = index + 3
            continue
        }

        if first < lead_4_limit {
            if index + 3 >= bytes.len {
                return false
            }
            second_4: u8 = bytes[index + 1]
            third_4: u8 = bytes[index + 2]
            fourth_4: u8 = bytes[index + 3]
            if second_4 < cont_lo {
                return false
            }
            if second_4 >= cont_hi {
                return false
            }
            if third_4 < cont_lo {
                return false
            }
            if third_4 >= cont_hi {
                return false
            }
            if fourth_4 < cont_lo {
                return false
            }
            if fourth_4 >= cont_hi {
                return false
            }
            if first == lead_f0 {
                if second_4 < second_f0_min {
                    return false
                }
            }
            if first == lead_f4 {
                if second_4 >= second_f0_min {
                    return false
                }
            }
            index = index + 4
            continue
        }

        return false
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