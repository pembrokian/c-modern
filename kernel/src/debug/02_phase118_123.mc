func validate_phase118_request_reply_and_delegation_follow_through(audit: Phase118DelegatedRequestReplyAudit, scheduler_contract_hardened: u32, lifecycle_contract_hardened: u32, capability_contract_hardened: u32, ipc_contract_hardened: u32, address_space_contract_hardened: u32, interrupt_contract_hardened: u32, timer_contract_hardened: u32, barrier_contract_hardened: u32) bool {
    if !validate_phase117_init_orchestrated_multi_service_bring_up(audit.phase117, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened, barrier_contract_hardened) {
        return false
    }
    if audit.transfer_service_transfer.grant_count != 1 {
        return false
    }
    if audit.transfer_service_transfer.emit_count != 1 {
        return false
    }
    if audit.transfer_service_transfer.grant_len != audit.transfer_service_transfer.emit_len {
        return false
    }
    if audit.transfer_service_transfer.grant_byte0 != audit.transfer_service_transfer.emit_byte0 {
        return false
    }
    if audit.transfer_service_transfer.grant_byte1 != audit.transfer_service_transfer.emit_byte1 {
        return false
    }
    if audit.transfer_service_transfer.grant_byte2 != audit.transfer_service_transfer.emit_byte2 {
        return false
    }
    if audit.transfer_service_transfer.grant_byte3 != audit.transfer_service_transfer.emit_byte3 {
        return false
    }
    if syscall.status_score(audit.invalidated_source_send_status) != 8 {
        return false
    }
    if audit.invalidated_source_handle_slot == 0 {
        return false
    }
    if audit.retained_receive_handle_slot == 0 {
        return false
    }
    if audit.invalidated_source_handle_slot == audit.retained_receive_handle_slot {
        return false
    }
    if audit.retained_receive_endpoint_id != audit.phase117.transfer_endpoint_id {
        return false
    }
    return true
}

func validate_phase119_namespace_pressure_audit(audit: Phase119NamespacePressureAudit, scheduler_contract_hardened: u32, lifecycle_contract_hardened: u32, capability_contract_hardened: u32, ipc_contract_hardened: u32, address_space_contract_hardened: u32, interrupt_contract_hardened: u32, timer_contract_hardened: u32, barrier_contract_hardened: u32) bool {
    if !validate_phase118_request_reply_and_delegation_follow_through(audit.phase118, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened, barrier_contract_hardened) {
        return false
    }
    if audit.directory_owner_pid != audit.phase118.phase117.running_slice.init_pid {
        return false
    }
    if audit.directory_entry_count != 3 {
        return false
    }
    if audit.log_service_key != 1 {
        return false
    }
    if audit.echo_service_key != 2 {
        return false
    }
    if audit.transfer_service_key != 3 {
        return false
    }
    if audit.log_service_key == audit.echo_service_key {
        return false
    }
    if audit.log_service_key == audit.transfer_service_key {
        return false
    }
    if audit.echo_service_key == audit.transfer_service_key {
        return false
    }
    if audit.shared_directory_endpoint_id != audit.phase118.phase117.init_endpoint_id {
        return false
    }
    if audit.shared_directory_endpoint_id == audit.phase118.phase117.transfer_endpoint_id {
        return false
    }
    if audit.log_service_program_slot == 0 {
        return false
    }
    if audit.echo_service_program_slot == 0 {
        return false
    }
    if audit.transfer_service_program_slot == 0 {
        return false
    }
    if audit.log_service_program_slot == audit.echo_service_program_slot {
        return false
    }
    if audit.log_service_program_slot == audit.transfer_service_program_slot {
        return false
    }
    if audit.echo_service_program_slot == audit.transfer_service_program_slot {
        return false
    }
    if audit.log_service_program_object_id == 0 {
        return false
    }
    if audit.echo_service_program_object_id == 0 {
        return false
    }
    if audit.transfer_service_program_object_id == 0 {
        return false
    }
    if audit.log_service_program_object_id == audit.echo_service_program_object_id {
        return false
    }
    if audit.log_service_program_object_id == audit.transfer_service_program_object_id {
        return false
    }
    if audit.echo_service_program_object_id == audit.transfer_service_program_object_id {
        return false
    }
    if audit.log_service_wait_handle_slot != audit.phase118.phase117.log_service_spawn.wait_handle_slot {
        return false
    }
    if audit.echo_service_wait_handle_slot != audit.phase118.phase117.echo_service_spawn.wait_handle_slot {
        return false
    }
    if audit.transfer_service_wait_handle_slot != audit.phase118.phase117.transfer_service_spawn.wait_handle_slot {
        return false
    }
    if audit.phase118.phase117.log_service_handshake.endpoint_id != audit.shared_directory_endpoint_id {
        return false
    }
    if audit.phase118.phase117.echo_service_exchange.endpoint_id != audit.shared_directory_endpoint_id {
        return false
    }
    if audit.phase118.phase117.transfer_service_transfer.control_endpoint_id != audit.shared_directory_endpoint_id {
        return false
    }
    return audit.dynamic_namespace_visible == 0
}

func validate_phase120_running_system_support_statement(audit: Phase120RunningSystemSupportAudit, scheduler_contract_hardened: u32, lifecycle_contract_hardened: u32, capability_contract_hardened: u32, ipc_contract_hardened: u32, address_space_contract_hardened: u32, interrupt_contract_hardened: u32, timer_contract_hardened: u32, barrier_contract_hardened: u32) bool {
    if !validate_phase119_namespace_pressure_audit(audit.phase119, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened, barrier_contract_hardened) {
        return false
    }
    if audit.service_policy_owner_pid != audit.phase119.directory_owner_pid {
        return false
    }
    if audit.running_service_count != audit.phase119.directory_entry_count {
        return false
    }
    if audit.running_service_count != 3 {
        return false
    }
    if audit.fixed_directory_count != 1 {
        return false
    }
    if audit.shared_control_endpoint_id != audit.phase119.shared_directory_endpoint_id {
        return false
    }
    if audit.retained_reply_endpoint_id != audit.phase119.phase118.retained_receive_endpoint_id {
        return false
    }
    if audit.retained_reply_endpoint_id != audit.phase119.phase118.phase117.transfer_endpoint_id {
        return false
    }
    if audit.program_capability_count != 3 {
        return false
    }
    if audit.wait_handle_count != 3 {
        return false
    }
    if audit.dynamic_loading_visible != 0 {
        return false
    }
    if audit.service_manager_visible != 0 {
        return false
    }
    return audit.dynamic_namespace_visible == 0
}

func validate_phase121_kernel_image_contract_hardening(audit: Phase121KernelImageContractAudit, scheduler_contract_hardened: u32, lifecycle_contract_hardened: u32, capability_contract_hardened: u32, ipc_contract_hardened: u32, address_space_contract_hardened: u32, interrupt_contract_hardened: u32, timer_contract_hardened: u32, barrier_contract_hardened: u32) bool {
    if !validate_phase120_running_system_support_statement(audit.phase120, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened, barrier_contract_hardened) {
        return false
    }
    if audit.kernel_manifest_visible != 1 {
        return false
    }
    if audit.kernel_target_visible != 1 {
        return false
    }
    if audit.kernel_runtime_startup_visible != 1 {
        return false
    }
    if audit.bootstrap_target_family_visible != 1 {
        return false
    }
    if audit.emitted_image_input_visible != 1 {
        return false
    }
    if audit.linked_kernel_executable_visible != 1 {
        return false
    }
    if audit.dynamic_loading_visible != 0 {
        return false
    }
    if audit.service_manager_visible != 0 {
        return false
    }
    return audit.dynamic_namespace_visible == 0
}

func validate_phase122_target_surface_audit(audit: Phase122TargetSurfaceAudit, scheduler_contract_hardened: u32, lifecycle_contract_hardened: u32, capability_contract_hardened: u32, ipc_contract_hardened: u32, address_space_contract_hardened: u32, interrupt_contract_hardened: u32, timer_contract_hardened: u32, barrier_contract_hardened: u32) bool {
    if !validate_phase121_kernel_image_contract_hardening(audit.phase121, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened, barrier_contract_hardened) {
        return false
    }
    if audit.kernel_target_visible != 1 {
        return false
    }
    if audit.kernel_runtime_startup_visible != 1 {
        return false
    }
    if audit.bootstrap_target_family_visible != 1 {
        return false
    }
    if audit.bootstrap_target_family_only_visible != 1 {
        return false
    }
    if audit.broader_target_family_visible != 0 {
        return false
    }
    if audit.dynamic_loading_visible != 0 {
        return false
    }
    if audit.service_manager_visible != 0 {
        return false
    }
    return audit.dynamic_namespace_visible == 0
}

func validate_phase123_next_plateau_audit(audit: Phase123NextPlateauAudit, scheduler_contract_hardened: u32, lifecycle_contract_hardened: u32, capability_contract_hardened: u32, ipc_contract_hardened: u32, address_space_contract_hardened: u32, interrupt_contract_hardened: u32, timer_contract_hardened: u32, barrier_contract_hardened: u32) bool {
    if !validate_phase122_target_surface_audit(audit.phase122, scheduler_contract_hardened, lifecycle_contract_hardened, capability_contract_hardened, ipc_contract_hardened, address_space_contract_hardened, interrupt_contract_hardened, timer_contract_hardened, barrier_contract_hardened) {
        return false
    }
    if audit.running_kernel_truth_visible != 1 {
        return false
    }
    if audit.running_system_truth_visible != 1 {
        return false
    }
    if audit.kernel_image_truth_visible != 1 {
        return false
    }
    if audit.target_surface_truth_visible != 1 {
        return false
    }
    if audit.broader_platform_visible != 0 {
        return false
    }
    if audit.broad_target_support_visible != 0 {
        return false
    }
    if audit.general_loading_visible != 0 {
        return false
    }
    return audit.compiler_reopening_visible == 0
}

