export { main, parse_config }

import errors
import text

func parse_config(input: str) errors.Error {
    rest: str = input
    line_no: usize = 1
    port: usize = 0
    mode: str = ""

    while rest.len != 0 {
        newline: usize = text.find_byte(rest, 10)
        raw_line: str = rest
        has_more: bool = false
        if newline != rest.len {
            raw_line = rest[0:newline]
            rest = rest[newline + 1:rest.len]
            has_more = true
        } else {
            rest = rest[0:0]
        }

        line: str = text.trim_space(raw_line)
        if text.is_blank(line) {
            line_no = line_no + 1
            if !has_more {
                break
            }
            continue
        }

        equals: usize = text.find_byte(line, 61)
        if equals == line.len {
            return errors.fail_code(line_no)
        }

        key: str = text.trim_space(line[0:equals])
        value: str = text.trim_space(line[equals + 1:line.len])
        if text.eq(key, "port") {
            if !text.eq(value, "8080") {
                return errors.fail_code(line_no + 100)
            }
            port = 8080
        } else if text.eq(key, "mode") {
            mode = value
        } else {
            return errors.fail_code(line_no + 200)
        }

        line_no = line_no + 1
        if !has_more {
            break
        }
    }

    if port != 8080 {
        return errors.fail_code(300)
    }
    if !text.eq(mode, "fast") {
        return errors.fail_code(301)
    }

    return errors.ok()
}

func main() i32 {
    err: errors.Error = parse_config("port = 8080\nmode = fast\n")
    if !errors.is_ok(err) {
        return 1
    }
    return 0
}