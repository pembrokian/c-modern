import boot
import scenarios

func main() i32 {
    state := boot.kernel_init()
    return scenarios.run(&state)
}

