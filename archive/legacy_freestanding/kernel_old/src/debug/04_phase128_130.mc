import syscall

func validate_phase128_service_death_observation(audit: Phase128ServiceDeathObservationAudit, scheduler_contract_hardened: u32, lifecycle_contract_hardened: u32, capability_contract_hardened: u32, ipc_contract_hardened: u32, address_space_contract_hardened: u32, interrupt_contract_hardened: u32, timer_contract_hardened: u32, barrier_contract_hardened: u32) bool {
    if !validate_phase126_authority_lifetime_classification(audit.phase126, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened, barrier_contract_hardened) {
        return false
    }
    if syscall.status_score(audit.phase126.phase125.phase124.phase123.phase122.phase121.phase120.phase119.phase118.phase117.log_service_wait.status) != 2 {
        return false
    }
    if audit.observed_service_pid == 0 {
        return false
    }
    if audit.observed_service_pid != audit.phase126.phase125.phase124.phase123.phase122.phase121.phase120.phase119.phase118.phase117.log_service_wait.child_pid {
        return false
    }
    if audit.observed_service_pid != audit.phase126.phase125.phase124.phase123.phase122.phase121.phase120.phase119.phase118.phase117.log_service_spawn.child_pid {
        return false
    }
    if audit.observed_service_key != audit.phase126.phase125.phase124.phase123.phase122.phase121.phase120.phase119.log_service_key {
        return false
    }
    if audit.observed_service_key == 0 {
        return false
    }
    if audit.observed_wait_handle_slot != audit.phase126.phase125.phase124.phase123.phase122.phase121.phase120.phase119.log_service_wait_handle_slot {
        return false
    }
    if audit.observed_wait_handle_slot != audit.phase126.phase125.phase124.phase123.phase122.phase121.phase120.phase119.phase118.phase117.log_service_wait.wait_handle_slot {
        return false
    }
    if audit.observed_wait_handle_slot == 0 {
        return false
    }
    if audit.observed_exit_code != audit.phase126.phase125.phase124.phase123.phase122.phase121.phase120.phase119.phase118.phase117.running_slice.log_service_exit_code {
        return false
    }
    if audit.observed_exit_code != audit.phase126.phase125.phase124.phase123.phase122.phase121.phase120.phase119.phase118.phase117.log_service_wait.exit_code {
        return false
    }
    if audit.fixed_directory_entry_count != audit.phase126.phase125.phase124.phase123.phase122.phase121.phase120.phase119.directory_entry_count {
        return false
    }
    if audit.fixed_directory_entry_count != 3 {
        return false
    }
    if audit.service_death_visible != 1 {
        return false
    }
    if audit.kernel_supervision_visible != 0 {
        return false
    }
    if audit.service_restart_visible != 0 {
        return false
    }
    if audit.broader_failure_framework_visible != 0 {
        return false
    }
    return audit.compiler_reopening_visible == 0
}

func validate_phase129_partial_failure_propagation(audit: Phase129PartialFailurePropagationAudit, scheduler_contract_hardened: u32, lifecycle_contract_hardened: u32, capability_contract_hardened: u32, ipc_contract_hardened: u32, address_space_contract_hardened: u32, interrupt_contract_hardened: u32, timer_contract_hardened: u32, barrier_contract_hardened: u32) bool {
    if !validate_phase128_service_death_observation(audit.phase128, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened, barrier_contract_hardened) {
        return false
    }
    if audit.failed_service_pid == 0 {
        return false
    }
    if audit.failed_service_pid != audit.phase128.observed_service_pid {
        return false
    }
    if audit.failed_service_key != audit.phase128.observed_service_key {
        return false
    }
    if audit.failed_wait_handle_slot != audit.phase128.observed_wait_handle_slot {
        return false
    }
    if syscall.status_score(audit.failed_wait_status) != 2 {
        return false
    }
    if audit.surviving_service_pid == 0 {
        return false
    }
    if audit.surviving_service_key != audit.phase128.phase126.phase125.phase124.phase123.phase122.phase121.phase120.phase119.echo_service_key {
        return false
    }
    if audit.surviving_service_key == audit.failed_service_key {
        return false
    }
    if audit.surviving_wait_handle_slot != audit.phase128.phase126.phase125.phase124.phase123.phase122.phase121.phase120.phase119.echo_service_wait_handle_slot {
        return false
    }
    if syscall.status_score(audit.surviving_reply_status) != 2 {
        return false
    }
    if syscall.status_score(audit.surviving_wait_status) != 2 {
        return false
    }
    if audit.surviving_reply_byte0 != audit.phase128.phase126.phase125.phase124.phase123.phase122.phase121.phase120.phase119.phase118.phase117.running_slice.echo_service_request_byte0 {
        return false
    }
    if audit.surviving_reply_byte1 != audit.phase128.phase126.phase125.phase124.phase123.phase122.phase121.phase120.phase119.phase118.phase117.running_slice.echo_service_request_byte1 {
        return false
    }
    if audit.shared_control_endpoint_id != audit.phase128.phase126.phase125.phase124.phase123.phase122.phase121.phase120.phase119.shared_directory_endpoint_id {
        return false
    }
    if audit.shared_control_endpoint_id == 0 {
        return false
    }
    if audit.directory_entry_count != audit.phase128.fixed_directory_entry_count {
        return false
    }
    if audit.directory_entry_count != 3 {
        return false
    }
    if audit.partial_failure_visible != 1 {
        return false
    }
    if audit.kernel_recovery_visible != 0 {
        return false
    }
    if audit.service_rebinding_visible != 0 {
        return false
    }
    if audit.broader_failure_framework_visible != 0 {
        return false
    }
    return audit.compiler_reopening_visible == 0
}

func validate_phase130_explicit_restart_or_replacement(audit: Phase130ExplicitRestartOrReplacementAudit, scheduler_contract_hardened: u32, lifecycle_contract_hardened: u32, capability_contract_hardened: u32, ipc_contract_hardened: u32, address_space_contract_hardened: u32, interrupt_contract_hardened: u32, timer_contract_hardened: u32, barrier_contract_hardened: u32) bool {
    if !validate_phase129_partial_failure_propagation(audit.phase129, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened, barrier_contract_hardened) {
        return false
    }
    if audit.replacement_policy_owner_pid != audit.phase129.phase128.phase126.phase125.phase124.phase123.phase122.phase121.phase120.service_policy_owner_pid {
        return false
    }
    if audit.replacement_policy_owner_pid == 0 {
        return false
    }
    if audit.replacement_service_pid == 0 {
        return false
    }
    if audit.replacement_service_pid != audit.phase129.failed_service_pid {
        return false
    }
    if audit.replacement_service_key != audit.phase129.failed_service_key {
        return false
    }
    if audit.replacement_service_key != audit.phase129.phase128.phase126.phase125.phase124.phase123.phase122.phase121.phase120.phase119.log_service_key {
        return false
    }
    if audit.replacement_wait_handle_slot != audit.phase129.phase128.phase126.phase125.phase124.phase123.phase122.phase121.phase120.phase119.log_service_wait_handle_slot {
        return false
    }
    if audit.replacement_wait_handle_slot == 0 {
        return false
    }
    if audit.replacement_program_slot != audit.phase129.phase128.phase126.phase125.phase124.phase123.phase122.phase121.phase120.phase119.log_service_program_slot {
        return false
    }
    if audit.replacement_program_object_id != audit.phase129.phase128.phase126.phase125.phase124.phase123.phase122.phase121.phase120.phase119.log_service_program_object_id {
        return false
    }
    if syscall.status_score(audit.replacement_spawn_status) != 2 {
        return false
    }
    if syscall.status_score(audit.replacement_ack_status) != 2 {
        return false
    }
    if syscall.status_score(audit.replacement_wait_status) != 2 {
        return false
    }
    if audit.replacement_ack_byte != audit.phase129.phase128.phase126.phase125.phase124.phase123.phase122.phase121.phase120.phase119.phase118.phase117.log_service_handshake.ack_byte {
        return false
    }
    if audit.shared_control_endpoint_id != audit.phase129.shared_control_endpoint_id {
        return false
    }
    if audit.shared_control_endpoint_id == 0 {
        return false
    }
    if audit.directory_entry_count != audit.phase129.directory_entry_count {
        return false
    }
    if audit.directory_entry_count != 3 {
        return false
    }
    if audit.explicit_restart_or_replacement_visible != 1 {
        return false
    }
    if audit.kernel_supervision_visible != 0 {
        return false
    }
    if audit.service_rebinding_visible != 0 {
        return false
    }
    if audit.broader_failure_framework_visible != 0 {
        return false
    }
    return audit.compiler_reopening_visible == 0
}