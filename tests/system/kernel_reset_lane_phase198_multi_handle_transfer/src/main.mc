import boot
import scenario_transfer

func main() i32 {
    state: boot.KernelBootState = boot.kernel_init()
    return scenario_transfer.run_transfer_probe(&state)
}