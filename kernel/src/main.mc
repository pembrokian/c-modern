import boot
import scenarios

func main() i32 {
    state: boot.KernelBootState = boot.kernel_init()
    return scenarios.run(&state)
}

