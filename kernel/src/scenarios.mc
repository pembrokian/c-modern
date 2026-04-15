// Scenario orchestration entry point.

import boot
import scenario_lifecycle
import scenario_queue
import scenario_restart
import scenario_steps

func run(state: *boot.KernelBootState) i32 {
    result: i32 = scenario_steps.run_main(state)
    if result != 0 {
        return result
    }
    result = scenario_restart.run_restart_probe(state)
    if result != 0 {
        return result
    }
    result = scenario_lifecycle.run_shell_lifecycle_probe(state)
    if result != 0 {
        return result
    }
    return scenario_queue.run_queue_observability_probe(state)
}
