export { main }

import helper_types

func main() i32 {
    pair: helper_types.Pair = helper_types.Pair{ left: 2, right: 5 }
    return pair.left + pair.right
}