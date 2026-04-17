// Scenario orchestration entry point.

import boot
import scenario_authority
import scenario_audit_coordination
import scenario_coordination
import scenario_lifecycle
import scenario_retained_policy
import scenario_queue
import scenario_retained_summary
import scenario_restart
import scenario_steps
import scenario_transfer
import scenario_workset_identity

func run(state: *boot.KernelBootState) i32 {
    result := scenario_steps.run_main(state)
    if result != 0 {
        return result
    }
    result = scenario_restart.run_restart_probe(state)
    if result != 0 {
        return result
    }
    transfer_state := boot.kernel_init()
    result = scenario_transfer.run_transfer_probe(&transfer_state)
    if result != 0 {
        return result
    }
    result = scenario_lifecycle.run_shell_lifecycle_probe(state)
    if result != 0 {
        return result
    }
    result = scenario_queue.run_queue_observability_probe(state)
    if result != 0 {
        return result
    }
    audit_state := boot.kernel_init()
    result = scenario_audit_coordination.run_retained_audit_coordination_probe(&audit_state)
    if result != 0 {
        return result
    }
    result = scenario_coordination.run_retained_coordination_probe(state)
    if result != 0 {
        return result
    }
    result = scenario_workset_identity.run_workset_identity_probe(state)
    if result != 0 {
        return result
    }
    result = scenario_retained_policy.run_retained_policy_probe()
    if result != 0 {
        return result
    }
    result = scenario_authority.run_authority_probe()
    if result != 0 {
        return result
    }
    return scenario_retained_summary.run_retained_summary_probe()
}
