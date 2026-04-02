export { main }

import helper_aliases

func main() i32 {
    value: helper_aliases.Counter = 4
    return helper_aliases.add_one(value)
}