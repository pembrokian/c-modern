func seed_serial_service_program_capability(config: SerialServiceConfig, execution: SerialServiceExecutionState) SerialServiceExecutionState {
    return SerialServiceExecutionState{ program_capability: capability.CapabilitySlot{ slot_id: config.program_slot, owner_pid: config.init_pid, kind: capability.CapabilityKind.InitProgram, rights: 7, object_id: config.program_object_id }, gate: execution.gate, process_slots: execution.process_slots, task_slots: execution.task_slots, init_handle_table: execution.init_handle_table, child_handle_table: execution.child_handle_table, wait_table: execution.wait_table, endpoints: execution.endpoints, init_image: execution.init_image, child_address_space: execution.child_address_space, child_user_frame: execution.child_user_frame, service_state: execution.service_state, spawn_observation: execution.spawn_observation, receive_observation: execution.receive_observation, ingress: execution.ingress, wait_observation: execution.wait_observation, ready_queue: execution.ready_queue }
}

func ensure_serial_service_endpoint(config: SerialServiceConfig, execution: SerialServiceExecutionState) SerialServiceExecutionState {
    if ipc.resolve_runtime_endpoint(execution.endpoints, config.serial_endpoint_id).valid != 0 {
        return execution
    }
    return SerialServiceExecutionState{ program_capability: execution.program_capability, gate: execution.gate, process_slots: execution.process_slots, task_slots: execution.task_slots, init_handle_table: execution.init_handle_table, child_handle_table: execution.child_handle_table, wait_table: execution.wait_table, endpoints: ipc.install_endpoint(execution.endpoints, config.init_pid, config.serial_endpoint_id), init_image: execution.init_image, child_address_space: execution.child_address_space, child_user_frame: execution.child_user_frame, service_state: execution.service_state, spawn_observation: execution.spawn_observation, receive_observation: execution.receive_observation, ingress: execution.ingress, wait_observation: execution.wait_observation, ready_queue: execution.ready_queue }
}

func spawn_serial_service(config: SerialServiceConfig, execution: SerialServiceExecutionState) SerialServiceExecutionResult {
    next_state: SerialServiceExecutionState = ensure_serial_service_endpoint(config, execution)
    child_handles: capability.HandleTable = capability.handle_table_for_owner(config.child_pid)
    wait_table: capability.WaitTable = capability.wait_table_for_owner(config.init_pid)
    spawn_request: syscall.SpawnRequest = syscall.build_spawn_request(config.wait_handle_slot)
    spawn_result: syscall.SpawnResult = syscall.perform_spawn(next_state.gate, next_state.program_capability, next_state.process_slots, next_state.task_slots, wait_table, next_state.init_image, spawn_request, config.child_pid, config.child_tid, config.child_asid, config.child_translation_root)
    child_handles = capability.install_endpoint_handle(child_handles, config.endpoint_handle_slot, config.serial_endpoint_id, 3)
    next_state = SerialServiceExecutionState{ program_capability: spawn_result.program_capability, gate: spawn_result.gate, process_slots: spawn_result.process_slots, task_slots: spawn_result.task_slots, init_handle_table: next_state.init_handle_table, child_handle_table: child_handles, wait_table: spawn_result.wait_table, endpoints: next_state.endpoints, init_image: next_state.init_image, child_address_space: spawn_result.child_address_space, child_user_frame: spawn_result.child_frame, service_state: serial_service.service_state(config.child_pid, config.endpoint_handle_slot), spawn_observation: spawn_result.observation, receive_observation: next_state.receive_observation, ingress: next_state.ingress, wait_observation: next_state.wait_observation, ready_queue: state.user_ready_queue(config.child_tid) }
    if syscall.status_score(next_state.spawn_observation.status) != 2 {
        return serial_service_result(next_state, 0)
    }
    if capability.find_endpoint_for_handle(next_state.child_handle_table, config.endpoint_handle_slot) != config.serial_endpoint_id {
        return serial_service_result(next_state, 0)
    }
    return serial_service_result(next_state, 1)
}

func execute_serial_service_receive_and_exit(config: SerialServiceConfig, execution: SerialServiceExecutionState) SerialServiceExecutionResult {
    service_receive: syscall.ReceiveResult = syscall.perform_receive(execution.gate, execution.child_handle_table, execution.endpoints, syscall.build_receive_request(config.endpoint_handle_slot))
    updated_service_state: serial_service.SerialServiceState = serial_service.record_ingress(execution.service_state, service_receive.observation)
    next_state: SerialServiceExecutionState = SerialServiceExecutionState{ program_capability: execution.program_capability, gate: service_receive.gate, process_slots: execution.process_slots, task_slots: execution.task_slots, init_handle_table: execution.init_handle_table, child_handle_table: service_receive.handle_table, wait_table: execution.wait_table, endpoints: service_receive.endpoints, init_image: execution.init_image, child_address_space: execution.child_address_space, child_user_frame: execution.child_user_frame, service_state: updated_service_state, spawn_observation: execution.spawn_observation, receive_observation: service_receive.observation, ingress: serial_service.observe_ingress(updated_service_state), wait_observation: execution.wait_observation, ready_queue: execution.ready_queue }
    if syscall.status_score(next_state.receive_observation.status) != 2 {
        return serial_service_result(next_state, 0)
    }

    next_state = simulate_serial_service_exit(config, next_state)
    wait_result: syscall.WaitResult = syscall.perform_wait(next_state.gate, next_state.process_slots, next_state.task_slots, next_state.wait_table, syscall.build_wait_request(config.wait_handle_slot))
    next_state = SerialServiceExecutionState{ program_capability: next_state.program_capability, gate: wait_result.gate, process_slots: wait_result.process_slots, task_slots: wait_result.task_slots, init_handle_table: next_state.init_handle_table, child_handle_table: capability.empty_handle_table(), wait_table: wait_result.wait_table, endpoints: next_state.endpoints, init_image: next_state.init_image, child_address_space: next_state.child_address_space, child_user_frame: next_state.child_user_frame, service_state: next_state.service_state, spawn_observation: next_state.spawn_observation, receive_observation: next_state.receive_observation, ingress: next_state.ingress, wait_observation: wait_result.observation, ready_queue: state.empty_queue() }
    if syscall.status_score(next_state.wait_observation.status) != 2 {
        return serial_service_result(next_state, 0)
    }
    return serial_service_result(next_state, 1)
}

func simulate_serial_service_exit(config: SerialServiceConfig, execution: SerialServiceExecutionState) SerialServiceExecutionState {
    processes: [3]state.ProcessSlot = lifecycle.exit_process(execution.process_slots, 2)
    tasks: [3]state.TaskSlot = lifecycle.exit_task(execution.task_slots, 2)
    wait_table: capability.WaitTable = capability.mark_wait_handle_exited(execution.wait_table, config.child_pid, config.exit_code)
    return SerialServiceExecutionState{ program_capability: execution.program_capability, gate: execution.gate, process_slots: processes, task_slots: tasks, init_handle_table: execution.init_handle_table, child_handle_table: execution.child_handle_table, wait_table: wait_table, endpoints: execution.endpoints, init_image: execution.init_image, child_address_space: execution.child_address_space, child_user_frame: execution.child_user_frame, service_state: execution.service_state, spawn_observation: execution.spawn_observation, receive_observation: execution.receive_observation, ingress: execution.ingress, wait_observation: execution.wait_observation, ready_queue: state.empty_queue() }
}

func execute_phase134_serial_service_handoff(config: SerialServiceConfig, execution: SerialServiceExecutionState) SerialServiceExecutionResult {
    next_state: SerialServiceExecutionState = seed_serial_service_program_capability(config, execution)
    if !capability.is_program_capability(next_state.program_capability) {
        return serial_service_result(next_state, 0)
    }
    spawned: SerialServiceExecutionResult = spawn_serial_service(config, next_state)
    if spawned.succeeded == 0 {
        return spawned
    }
    return execute_serial_service_receive_and_exit(config, spawned.state)
}