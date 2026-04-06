export { main }

import io

func main(args: Slice<cstr>) i32 {
    if io.write_line("hello, phase8") != 0 {
        return 1
    }
    return 0
}