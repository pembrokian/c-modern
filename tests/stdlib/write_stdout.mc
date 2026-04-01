export { main }

import io

func main() i32 {
    if io.write("phase6") != 0 {
        return 1
    }
    if io.write(" write") != 0 {
        return 2
    }
    return 0
}