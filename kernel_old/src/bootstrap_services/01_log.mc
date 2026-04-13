func seed_log_service_program_capability(config: LogServiceConfig, execution: LogServiceExecutionState) LogServiceExecutionState {
    return LogServiceExecutionState{ program_capability: capability.CapabilitySlot{ slot_id: config.program_slot, owner_pid: config.init_pid, kind: capability.CapabilityKind.InitProgram, rights: 7, object_id: config.program_object_id }, gate: execution.gate, process_slots: execution.process_slots, task_slots: execution.task_slots, init_handle_table: execution.init_handle_table, child_handle_table: execution.child_handle_table, wait_table: execution.wait_table, endpoints: execution.endpoints, init_image: execution.init_image, child_address_space: execution.child_address_space, child_user_frame: execution.child_user_frame, service_state: execution.service_state, spawn_observation: execution.spawn_observation, receive_observation: execution.receive_observation, ack_observation: execution.ack_observation, handshake: execution.handshake, wait_observation: execution.wait_observation, ready_queue: execution.ready_queue }
}

func spawn_log_service(config: LogServiceConfig, execution: LogServiceExecutionState) LogServiceExecutionResult {
    child_handles: capability.HandleTable = capability.handle_table_for_owner(config.child_pid)
    wait_table: capability.WaitTable = capability.wait_table_for_owner(config.init_pid)

    spawn_request: syscall.SpawnRequest = syscall.build_spawn_request(config.wait_handle_slot)
    spawn_result: syscall.SpawnResult = syscall.perform_spawn(execution.gate, execution.program_capability, execution.process_slots, execution.task_slots, wait_table, execution.init_image, spawn_request, config.child_pid, config.child_tid, config.child_asid, config.child_translation_root)
    next_state: LogServiceExecutionState = LogServiceExecutionState{ program_capability: spawn_result.program_capability, gate: spawn_result.gate, process_slots: spawn_result.process_slots, task_slots: spawn_result.task_slots, init_handle_table: execution.init_handle_table, child_handle_table: child_handles, wait_table: spawn_result.wait_table, endpoints: execution.endpoints, init_image: execution.init_image, child_address_space: spawn_result.child_address_space, child_user_frame: spawn_result.child_frame, service_state: execution.service_state, spawn_observation: spawn_result.observation, receive_observation: execution.receive_observation, ack_observation: execution.ack_observation, handshake: execution.handshake, wait_observation: execution.wait_observation, ready_queue: execution.ready_queue }
    if syscall.status_score(next_state.spawn_observation.status) != 2 {
        return log_service_result(next_state, 0)
    }

    child_handles = capability.install_endpoint_handle(next_state.child_handle_table, config.endpoint_handle_slot, config.init_endpoint_id, 3)
    next_state = LogServiceExecutionState{ program_capability: next_state.program_capability, gate: next_state.gate, process_slots: next_state.process_slots, task_slots: next_state.task_slots, init_handle_table: next_state.init_handle_table, child_handle_table: child_handles, wait_table: next_state.wait_table, endpoints: next_state.endpoints, init_image: next_state.init_image, child_address_space: next_state.child_address_space, child_user_frame: next_state.child_user_frame, service_state: log_service.service_state(config.child_pid, config.endpoint_handle_slot), spawn_observation: next_state.spawn_observation, receive_observation: next_state.receive_observation, ack_observation: next_state.ack_observation, handshake: next_state.handshake, wait_observation: next_state.wait_observation, ready_queue: state.user_ready_queue(config.child_tid) }
    if capability.find_endpoint_for_handle(next_state.child_handle_table, config.endpoint_handle_slot) != config.init_endpoint_id {
        return log_service_result(next_state, 0)
    }
    return log_service_result(next_state, 1)
}

func execute_log_service_request_reply(config: LogServiceConfig, execution: LogServiceExecutionState) LogServiceExecutionResult {
    request_payload: [4]u8 = ipc.zero_payload()
    request_payload[0] = config.request_byte

    request_send_result: syscall.SendResult = syscall.perform_send(execution.gate, execution.init_handle_table, execution.endpoints, config.init_pid, syscall.build_send_request(config.init_endpoint_handle_slot, 1, request_payload))
    next_state: LogServiceExecutionState = LogServiceExecutionState{ program_capability: execution.program_capability, gate: request_send_result.gate, process_slots: execution.process_slots, task_slots: execution.task_slots, init_handle_table: request_send_result.handle_table, child_handle_table: execution.child_handle_table, wait_table: execution.wait_table, endpoints: request_send_result.endpoints, init_image: execution.init_image, child_address_space: execution.child_address_space, child_user_frame: execution.child_user_frame, service_state: execution.service_state, spawn_observation: execution.spawn_observation, receive_observation: execution.receive_observation, ack_observation: execution.ack_observation, handshake: execution.handshake, wait_observation: execution.wait_observation, ready_queue: execution.ready_queue }
    if syscall.status_score(request_send_result.status) != 2 {
        return log_service_result(next_state, 0)
    }

    service_receive_result: syscall.ReceiveResult = syscall.perform_receive(next_state.gate, next_state.child_handle_table, next_state.endpoints, syscall.build_receive_request(config.endpoint_handle_slot))
    next_state = LogServiceExecutionState{ program_capability: next_state.program_capability, gate: service_receive_result.gate, process_slots: next_state.process_slots, task_slots: next_state.task_slots, init_handle_table: next_state.init_handle_table, child_handle_table: service_receive_result.handle_table, wait_table: next_state.wait_table, endpoints: service_receive_result.endpoints, init_image: next_state.init_image, child_address_space: next_state.child_address_space, child_user_frame: next_state.child_user_frame, service_state: next_state.service_state, spawn_observation: next_state.spawn_observation, receive_observation: service_receive_result.observation, ack_observation: next_state.ack_observation, handshake: next_state.handshake, wait_observation: next_state.wait_observation, ready_queue: next_state.ready_queue }
    if syscall.status_score(next_state.receive_observation.status) != 2 {
        return log_service_result(next_state, 0)
    }
    updated_service_state: log_service.LogServiceState = log_service.record_open_request(next_state.service_state, next_state.receive_observation)

    ack_send_result: syscall.SendResult = syscall.perform_send(next_state.gate, next_state.child_handle_table, next_state.endpoints, config.child_pid, syscall.build_send_request(config.endpoint_handle_slot, 1, log_service.ack_payload()))
    next_state = LogServiceExecutionState{ program_capability: next_state.program_capability, gate: ack_send_result.gate, process_slots: next_state.process_slots, task_slots: next_state.task_slots, init_handle_table: next_state.init_handle_table, child_handle_table: ack_send_result.handle_table, wait_table: next_state.wait_table, endpoints: ack_send_result.endpoints, init_image: next_state.init_image, child_address_space: next_state.child_address_space, child_user_frame: next_state.child_user_frame, service_state: updated_service_state, spawn_observation: next_state.spawn_observation, receive_observation: next_state.receive_observation, ack_observation: next_state.ack_observation, handshake: next_state.handshake, wait_observation: next_state.wait_observation, ready_queue: next_state.ready_queue }
    if syscall.status_score(ack_send_result.status) != 2 {
        return log_service_result(next_state, 0)
    }

    init_receive_result: syscall.ReceiveResult = syscall.perform_receive(next_state.gate, next_state.init_handle_table, next_state.endpoints, syscall.build_receive_request(config.init_endpoint_handle_slot))
    updated_service_state = log_service.record_ack(next_state.service_state, init_receive_result.observation.payload[0])
    next_state = LogServiceExecutionState{ program_capability: next_state.program_capability, gate: init_receive_result.gate, process_slots: next_state.process_slots, task_slots: next_state.task_slots, init_handle_table: init_receive_result.handle_table, child_handle_table: next_state.child_handle_table, wait_table: next_state.wait_table, endpoints: init_receive_result.endpoints, init_image: next_state.init_image, child_address_space: next_state.child_address_space, child_user_frame: next_state.child_user_frame, service_state: updated_service_state, spawn_observation: next_state.spawn_observation, receive_observation: next_state.receive_observation, ack_observation: init_receive_result.observation, handshake: log_service.observe_handshake(updated_service_state), wait_observation: next_state.wait_observation, ready_queue: next_state.ready_queue }
    if syscall.status_score(next_state.ack_observation.status) != 2 {
        return log_service_result(next_state, 0)
    }
    return log_service_result(next_state, 1)
}

func simulate_log_service_exit(config: LogServiceConfig, execution: LogServiceExecutionState) LogServiceExecutionState {
    processes: [3]state.ProcessSlot = lifecycle.exit_process(execution.process_slots, 2)
    tasks: [3]state.TaskSlot = lifecycle.exit_task(execution.task_slots, 2)
    wait_table: capability.WaitTable = capability.mark_wait_handle_exited(execution.wait_table, config.child_pid, config.exit_code)
    return LogServiceExecutionState{ program_capability: execution.program_capability, gate: execution.gate, process_slots: processes, task_slots: tasks, init_handle_table: execution.init_handle_table, child_handle_table: execution.child_handle_table, wait_table: wait_table, endpoints: execution.endpoints, init_image: execution.init_image, child_address_space: execution.child_address_space, child_user_frame: execution.child_user_frame, service_state: execution.service_state, spawn_observation: execution.spawn_observation, receive_observation: execution.receive_observation, ack_observation: execution.ack_observation, handshake: execution.handshake, wait_observation: execution.wait_observation, ready_queue: state.empty_queue() }
}

func execute_phase105_log_service_handshake(config: LogServiceConfig, execution: LogServiceExecutionState) LogServiceExecutionResult {
    next_state: LogServiceExecutionState = seed_log_service_program_capability(config, execution)
    if !capability.is_program_capability(next_state.program_capability) {
        return log_service_result(next_state, 0)
    }

    spawned: LogServiceExecutionResult = spawn_log_service(config, next_state)
    if spawned.succeeded == 0 {
        return spawned
    }

    exchanged: LogServiceExecutionResult = execute_log_service_request_reply(config, spawned.state)
    if exchanged.succeeded == 0 {
        return exchanged
    }

    next_state = simulate_log_service_exit(config, exchanged.state)
    wait_result: syscall.WaitResult = syscall.perform_wait(next_state.gate, next_state.process_slots, next_state.task_slots, next_state.wait_table, syscall.build_wait_request(config.wait_handle_slot))
    next_state = LogServiceExecutionState{ program_capability: next_state.program_capability, gate: wait_result.gate, process_slots: wait_result.process_slots, task_slots: wait_result.task_slots, init_handle_table: next_state.init_handle_table, child_handle_table: capability.empty_handle_table(), wait_table: wait_result.wait_table, endpoints: next_state.endpoints, init_image: next_state.init_image, child_address_space: next_state.child_address_space, child_user_frame: next_state.child_user_frame, service_state: next_state.service_state, spawn_observation: next_state.spawn_observation, receive_observation: next_state.receive_observation, ack_observation: next_state.ack_observation, handshake: next_state.handshake, wait_observation: wait_result.observation, ready_queue: state.empty_queue() }
    if syscall.status_score(next_state.wait_observation.status) != 2 {
        return log_service_result(next_state, 0)
    }
    return log_service_result(next_state, 1)
}