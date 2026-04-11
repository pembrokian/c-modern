func seed_transfer_service_program_capability(config: TransferServiceConfig, execution: TransferServiceExecutionState) TransferServiceExecutionState {
    return TransferServiceExecutionState{ program_capability: capability.CapabilitySlot{ slot_id: config.program_slot, owner_pid: config.init_pid, kind: capability.CapabilityKind.InitProgram, rights: 7, object_id: config.program_object_id }, gate: execution.gate, process_slots: execution.process_slots, task_slots: execution.task_slots, init_handle_table: execution.init_handle_table, child_handle_table: execution.child_handle_table, wait_table: execution.wait_table, endpoints: execution.endpoints, init_image: execution.init_image, child_address_space: execution.child_address_space, child_user_frame: execution.child_user_frame, service_state: execution.service_state, spawn_observation: execution.spawn_observation, grant_observation: execution.grant_observation, emit_observation: execution.emit_observation, transfer: execution.transfer, wait_observation: execution.wait_observation, ready_queue: execution.ready_queue }
}

func seed_transfer_service_sender_handle(config: TransferServiceConfig, execution: TransferServiceExecutionState) TransferServiceExecutionResult {
    init_handle_table: capability.HandleTable = capability.install_endpoint_handle(execution.init_handle_table, config.source_handle_slot, 2, 7)
    next_state: TransferServiceExecutionState = TransferServiceExecutionState{ program_capability: execution.program_capability, gate: execution.gate, process_slots: execution.process_slots, task_slots: execution.task_slots, init_handle_table: init_handle_table, child_handle_table: execution.child_handle_table, wait_table: execution.wait_table, endpoints: execution.endpoints, init_image: execution.init_image, child_address_space: execution.child_address_space, child_user_frame: execution.child_user_frame, service_state: execution.service_state, spawn_observation: execution.spawn_observation, grant_observation: execution.grant_observation, emit_observation: execution.emit_observation, transfer: execution.transfer, wait_observation: execution.wait_observation, ready_queue: execution.ready_queue }
    if capability.find_endpoint_for_handle(next_state.init_handle_table, config.init_received_handle_slot) != 2 {
        return transfer_service_result(next_state, 0)
    }
    if capability.find_rights_for_handle(next_state.init_handle_table, config.init_received_handle_slot) != 7 {
        return transfer_service_result(next_state, 0)
    }
    if capability.find_endpoint_for_handle(next_state.init_handle_table, config.source_handle_slot) != 2 {
        return transfer_service_result(next_state, 0)
    }
    return transfer_service_result(next_state, 1)
}

func spawn_transfer_service(config: TransferServiceConfig, execution: TransferServiceExecutionState) TransferServiceExecutionResult {
    child_handles: capability.HandleTable = capability.handle_table_for_owner(config.child_pid)
    wait_table: capability.WaitTable = capability.wait_table_for_owner(config.init_pid)
    spawn_request: syscall.SpawnRequest = syscall.build_spawn_request(config.wait_handle_slot)
    spawn_result: syscall.SpawnResult = syscall.perform_spawn(execution.gate, execution.program_capability, execution.process_slots, execution.task_slots, wait_table, execution.init_image, spawn_request, config.child_pid, config.child_tid, config.child_asid, config.child_translation_root)
    next_state: TransferServiceExecutionState = TransferServiceExecutionState{ program_capability: spawn_result.program_capability, gate: spawn_result.gate, process_slots: spawn_result.process_slots, task_slots: spawn_result.task_slots, init_handle_table: execution.init_handle_table, child_handle_table: child_handles, wait_table: spawn_result.wait_table, endpoints: execution.endpoints, init_image: execution.init_image, child_address_space: spawn_result.child_address_space, child_user_frame: spawn_result.child_frame, service_state: execution.service_state, spawn_observation: spawn_result.observation, grant_observation: execution.grant_observation, emit_observation: execution.emit_observation, transfer: execution.transfer, wait_observation: execution.wait_observation, ready_queue: execution.ready_queue }
    if syscall.status_score(next_state.spawn_observation.status) != 2 {
        return transfer_service_result(next_state, 0)
    }
    child_handles = capability.install_endpoint_handle(next_state.child_handle_table, config.control_handle_slot, config.init_endpoint_id, 3)
    next_state = TransferServiceExecutionState{ program_capability: next_state.program_capability, gate: next_state.gate, process_slots: next_state.process_slots, task_slots: next_state.task_slots, init_handle_table: next_state.init_handle_table, child_handle_table: child_handles, wait_table: next_state.wait_table, endpoints: next_state.endpoints, init_image: next_state.init_image, child_address_space: next_state.child_address_space, child_user_frame: next_state.child_user_frame, service_state: transfer_service.service_state(config.child_pid, config.control_handle_slot, config.service_received_handle_slot), spawn_observation: next_state.spawn_observation, grant_observation: next_state.grant_observation, emit_observation: next_state.emit_observation, transfer: next_state.transfer, wait_observation: next_state.wait_observation, ready_queue: state.user_ready_queue(config.child_tid) }
    if capability.find_endpoint_for_handle(next_state.child_handle_table, config.control_handle_slot) != config.init_endpoint_id {
        return transfer_service_result(next_state, 0)
    }
    return transfer_service_result(next_state, 1)
}

func execute_user_to_user_capability_transfer(config: TransferServiceConfig, execution: TransferServiceExecutionState) TransferServiceExecutionResult {
    grant_payload: [4]u8 = endpoint.zero_payload()
    grant_payload[0] = config.grant_byte0
    grant_payload[1] = config.grant_byte1
    grant_payload[2] = config.grant_byte2
    grant_payload[3] = config.grant_byte3
    transfer_send_result: syscall.SendResult = syscall.perform_send(execution.gate, execution.init_handle_table, execution.endpoints, config.init_pid, syscall.build_transfer_send_request(config.init_endpoint_handle_slot, 4, grant_payload, config.source_handle_slot))
    next_state: TransferServiceExecutionState = TransferServiceExecutionState{ program_capability: execution.program_capability, gate: transfer_send_result.gate, process_slots: execution.process_slots, task_slots: execution.task_slots, init_handle_table: transfer_send_result.handle_table, child_handle_table: execution.child_handle_table, wait_table: execution.wait_table, endpoints: transfer_send_result.endpoints, init_image: execution.init_image, child_address_space: execution.child_address_space, child_user_frame: execution.child_user_frame, service_state: execution.service_state, spawn_observation: execution.spawn_observation, grant_observation: execution.grant_observation, emit_observation: execution.emit_observation, transfer: execution.transfer, wait_observation: execution.wait_observation, ready_queue: execution.ready_queue }
    if syscall.status_score(transfer_send_result.status) != 2 {
        return transfer_service_result(next_state, 0)
    }
    if capability.find_endpoint_for_handle(next_state.init_handle_table, config.source_handle_slot) != 0 {
        return transfer_service_result(next_state, 0)
    }
    if capability.find_endpoint_for_handle(next_state.init_handle_table, config.init_received_handle_slot) != 2 {
        return transfer_service_result(next_state, 0)
    }
    service_receive_result: syscall.ReceiveResult = syscall.perform_receive(next_state.gate, next_state.child_handle_table, next_state.endpoints, syscall.build_transfer_receive_request(config.control_handle_slot, config.service_received_handle_slot))
    next_state = TransferServiceExecutionState{ program_capability: next_state.program_capability, gate: service_receive_result.gate, process_slots: next_state.process_slots, task_slots: next_state.task_slots, init_handle_table: next_state.init_handle_table, child_handle_table: service_receive_result.handle_table, wait_table: next_state.wait_table, endpoints: service_receive_result.endpoints, init_image: next_state.init_image, child_address_space: next_state.child_address_space, child_user_frame: next_state.child_user_frame, service_state: next_state.service_state, spawn_observation: next_state.spawn_observation, grant_observation: service_receive_result.observation, emit_observation: next_state.emit_observation, transfer: next_state.transfer, wait_observation: next_state.wait_observation, ready_queue: next_state.ready_queue }
    if syscall.status_score(next_state.grant_observation.status) != 2 {
        return transfer_service_result(next_state, 0)
    }
    if capability.find_endpoint_for_handle(next_state.child_handle_table, config.service_received_handle_slot) != 2 {
        return transfer_service_result(next_state, 0)
    }
    transferred_endpoint_id: u32 = capability.find_endpoint_for_handle(next_state.child_handle_table, config.service_received_handle_slot)
    transferred_rights: u32 = capability.find_rights_for_handle(next_state.child_handle_table, config.service_received_handle_slot)
    updated_service_state: transfer_service.TransferServiceState = transfer_service.record_grant(next_state.service_state, next_state.grant_observation, transferred_endpoint_id, transferred_rights)
    emit_payload: [4]u8 = transfer_service.emit_payload(updated_service_state)
    emit_send_result: syscall.SendResult = syscall.perform_send(next_state.gate, next_state.child_handle_table, next_state.endpoints, config.child_pid, syscall.build_send_request(config.service_received_handle_slot, 4, emit_payload))
    next_state = TransferServiceExecutionState{ program_capability: next_state.program_capability, gate: emit_send_result.gate, process_slots: next_state.process_slots, task_slots: next_state.task_slots, init_handle_table: next_state.init_handle_table, child_handle_table: emit_send_result.handle_table, wait_table: next_state.wait_table, endpoints: emit_send_result.endpoints, init_image: next_state.init_image, child_address_space: next_state.child_address_space, child_user_frame: next_state.child_user_frame, service_state: updated_service_state, spawn_observation: next_state.spawn_observation, grant_observation: next_state.grant_observation, emit_observation: next_state.emit_observation, transfer: next_state.transfer, wait_observation: next_state.wait_observation, ready_queue: next_state.ready_queue }
    if syscall.status_score(emit_send_result.status) != 2 {
        return transfer_service_result(next_state, 0)
    }
    init_receive_result: syscall.ReceiveResult = syscall.perform_receive(next_state.gate, next_state.init_handle_table, next_state.endpoints, syscall.build_receive_request(config.init_received_handle_slot))
    updated_service_state = transfer_service.record_emit(next_state.service_state, init_receive_result.observation)
    next_state = TransferServiceExecutionState{ program_capability: next_state.program_capability, gate: init_receive_result.gate, process_slots: next_state.process_slots, task_slots: next_state.task_slots, init_handle_table: init_receive_result.handle_table, child_handle_table: next_state.child_handle_table, wait_table: next_state.wait_table, endpoints: init_receive_result.endpoints, init_image: next_state.init_image, child_address_space: next_state.child_address_space, child_user_frame: next_state.child_user_frame, service_state: updated_service_state, spawn_observation: next_state.spawn_observation, grant_observation: next_state.grant_observation, emit_observation: init_receive_result.observation, transfer: transfer_service.observe_transfer(updated_service_state), wait_observation: next_state.wait_observation, ready_queue: next_state.ready_queue }
    if syscall.status_score(next_state.emit_observation.status) != 2 {
        return transfer_service_result(next_state, 0)
    }
    return transfer_service_result(next_state, 1)
}

func simulate_transfer_service_exit(config: TransferServiceConfig, execution: TransferServiceExecutionState) TransferServiceExecutionState {
    processes: [3]state.ProcessSlot = lifecycle.exit_process(execution.process_slots, 2)
    tasks: [3]state.TaskSlot = lifecycle.exit_task(execution.task_slots, 2)
    wait_table: capability.WaitTable = capability.mark_wait_handle_exited(execution.wait_table, config.child_pid, config.exit_code)
    return TransferServiceExecutionState{ program_capability: execution.program_capability, gate: execution.gate, process_slots: processes, task_slots: tasks, init_handle_table: execution.init_handle_table, child_handle_table: execution.child_handle_table, wait_table: wait_table, endpoints: execution.endpoints, init_image: execution.init_image, child_address_space: execution.child_address_space, child_user_frame: execution.child_user_frame, service_state: execution.service_state, spawn_observation: execution.spawn_observation, grant_observation: execution.grant_observation, emit_observation: execution.emit_observation, transfer: execution.transfer, wait_observation: execution.wait_observation, ready_queue: state.empty_queue() }
}

func execute_phase107_user_to_user_capability_transfer(config: TransferServiceConfig, execution: TransferServiceExecutionState) TransferServiceExecutionResult {
    next_state: TransferServiceExecutionState = seed_transfer_service_program_capability(config, execution)
    if !capability.is_program_capability(next_state.program_capability) {
        return transfer_service_result(next_state, 0)
    }
    seeded: TransferServiceExecutionResult = seed_transfer_service_sender_handle(config, next_state)
    if seeded.succeeded == 0 {
        return seeded
    }
    spawned: TransferServiceExecutionResult = spawn_transfer_service(config, seeded.state)
    if spawned.succeeded == 0 {
        return spawned
    }
    transferred: TransferServiceExecutionResult = execute_user_to_user_capability_transfer(config, spawned.state)
    if transferred.succeeded == 0 {
        return transferred
    }
    next_state = simulate_transfer_service_exit(config, transferred.state)
    wait_result: syscall.WaitResult = syscall.perform_wait(next_state.gate, next_state.process_slots, next_state.task_slots, next_state.wait_table, syscall.build_wait_request(config.wait_handle_slot))
    next_state = TransferServiceExecutionState{ program_capability: next_state.program_capability, gate: wait_result.gate, process_slots: wait_result.process_slots, task_slots: wait_result.task_slots, init_handle_table: next_state.init_handle_table, child_handle_table: capability.empty_handle_table(), wait_table: wait_result.wait_table, endpoints: next_state.endpoints, init_image: next_state.init_image, child_address_space: next_state.child_address_space, child_user_frame: next_state.child_user_frame, service_state: next_state.service_state, spawn_observation: next_state.spawn_observation, grant_observation: next_state.grant_observation, emit_observation: next_state.emit_observation, transfer: next_state.transfer, wait_observation: wait_result.observation, ready_queue: state.empty_queue() }
    if syscall.status_score(next_state.wait_observation.status) != 2 {
        return transfer_service_result(next_state, 0)
    }
    return transfer_service_result(next_state, 1)
}