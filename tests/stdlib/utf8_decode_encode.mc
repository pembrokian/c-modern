import errors
import utf8

func main() i32 {
    decoded: utf8.Decoded
    err: errors.Error

    decoded, err = utf8.decode("é")
    if !errors.is_ok(err) {
        return 1
    }
    if decoded.codepoint != 233 {
        return 2
    }
    if decoded.width != 2 {
        return 3
    }

    encoded: utf8.Encoded
    encoded, err = utf8.encode(233)
    if !errors.is_ok(err) {
        return 4
    }
    if encoded.len != 2 {
        return 5
    }
    if encoded.bytes[0] != 195 {
        return 6
    }
    if encoded.bytes[1] != 169 {
        return 7
    }
    bad_decoded: utf8.Decoded
    bad_decoded, err = utf8.decode("")
    if errors.is_ok(err) {
        return 8
    }

    bad_encoded: utf8.Encoded
    bad_encoded, err = utf8.encode(55296)
    if errors.is_ok(err) {
        return 9
    }

    return 0
}