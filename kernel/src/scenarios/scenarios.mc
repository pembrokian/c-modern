// Scenario orchestration entry point.

import boot
import scenario_authority
import scenario_audit_coordination
import scenario_completion_mailbox
import scenario_coordination
import scenario_delegated_object_processing
import scenario_durability
import scenario_journal_service
import scenario_lease_service
import scenario_named_object_delivery_pressure
import scenario_lifecycle
import scenario_object_store_service
import scenario_object_update_workflow
import scenario_retained_policy
import scenario_queue
import scenario_retained_summary
import scenario_restart
import scenario_stall
import scenario_file_service
import scenario_timer_task_service
import scenario_workflow_service
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
    pressure_state := boot.kernel_init()
    result = scenario_queue.run_queue_pressure_probe(&pressure_state)
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
    result = scenario_durability.run_retained_durable_boundary_probe()
    if result != 0 {
        return result
    }
    result = scenario_retained_summary.run_retained_summary_probe()
    if result != 0 {
        return result
    }
    stall_state := boot.kernel_init()
    result = scenario_file_service.run_file_service_probe()
    if result != 0 {
        return result
    }
    result = scenario_timer_task_service.run_timer_task_probe()
    if result != 0 {
        return result
    }
    result = scenario_journal_service.run_journal_service_probe()
    if result != 0 {
        return result
    }
    result = scenario_object_store_service.run_object_store_service_probe()
    if result != 0 {
        return result
    }
    result = scenario_workflow_service.run_workflow_service_probe()
    if result != 0 {
        return result
    }
    result = scenario_object_update_workflow.run_named_object_update_workflow_probe()
    if result != 0 {
        return result
    }
    result = scenario_completion_mailbox.run_completion_mailbox_probe()
    if result != 0 {
        return result
    }
    result = scenario_lease_service.run_lease_service_probe()
    if result != 0 {
        return result
    }
    result = scenario_delegated_object_processing.run_delegated_named_object_processing_probe()
    if result != 0 {
        return result
    }
    result = scenario_named_object_delivery_pressure.run_named_object_delivery_pressure_probe()
    if result != 0 {
        return result
    }
    result = scenario_stall.run_stall_probe(&stall_state)
    if result != 0 {
        return result
    }
    return 0
}
