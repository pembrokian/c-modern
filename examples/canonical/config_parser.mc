export { main, parse_config }

import errors
import strings

const CONFIG_ERR_MISSING_EQUALS_BASE: usize = 0
const CONFIG_ERR_INVALID_PORT_BASE: usize = 100
const CONFIG_ERR_UNKNOWN_KEY_BASE: usize = 200
const CONFIG_ERR_MISSING_PORT: usize = 300
const CONFIG_ERR_INVALID_MODE: usize = 301

func parse_config(input: str) errors.Error {
    rest: str = input
    line_no: usize = 1
    port: usize = 0
    mode: str = ""

    while rest.len != 0 {
        newline: usize = strings.find_byte(rest, 10)
        raw_line: str = rest
        has_more: bool = false
        if newline != rest.len {
            raw_line = rest[0:newline]
            rest = rest[newline + 1:rest.len]
            has_more = true
        } else {
            rest = rest[0:0]
        }

        line: str = strings.trim_space(raw_line)
        if strings.is_blank(line) {
            line_no = line_no + 1
            if !has_more {
                break
            }
            continue
        }

        equals: usize = strings.find_byte(line, 61)
        if equals == line.len {
            return errors.fail_user(CONFIG_ERR_MISSING_EQUALS_BASE + line_no)
        }

        key: str = strings.trim_space(line[0:equals])
        value: str = strings.trim_space(line[equals + 1:line.len])
        if strings.eq(key, "port") {
            if !strings.eq(value, "8080") {
                return errors.fail_user(CONFIG_ERR_INVALID_PORT_BASE + line_no)
            }
            port = 8080
        } else if strings.eq(key, "mode") {
            mode = value
        } else {
            return errors.fail_user(CONFIG_ERR_UNKNOWN_KEY_BASE + line_no)
        }

        line_no = line_no + 1
        if !has_more {
            break
        }
    }

    if port != 8080 {
        return errors.fail_user(CONFIG_ERR_MISSING_PORT)
    }
    if !strings.eq(mode, "fast") {
        return errors.fail_user(CONFIG_ERR_INVALID_MODE)
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