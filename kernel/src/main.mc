import boot
import scenarios

func main() i32 {
    state: boot.KernelBootState = boot.kernel_init()

    if scenarios.run_basic_kv_log(&state) != 0 {
        return 1
    }
    if scenarios.run_multi_client(&state) != 0 {
        return 1
    }
    if scenarios.run_long_lived_coherence(&state) != 0 {
        return 1
    }
    if scenarios.run_cross_service_failure(&state) != 0 {
        return 1
    }
    if scenarios.run_observability_inspection(&state) != 0 {
        return 1
    }

    return 0
}

