func seed_serial_service_program_capability(config: SerialServiceConfig, execution: SerialServiceExecutionState) SerialServiceExecutionState {
    return SerialServiceExecutionState{ program_capability: capability.CapabilitySlot{ slot_id: config.program_slot, owner_pid: config.init_pid, kind: capability.CapabilityKind.InitProgram, rights: 7, object_id: config.program_object_id }, gate: execution.gate, process_slots: execution.process_slots, task_slots: execution.task_slots, init_handle_table: execution.init_handle_table, child_handle_table: execution.child_handle_table, wait_table: execution.wait_table, endpoints: execution.endpoints, init_image: execution.init_image, child_address_space: execution.child_address_space, child_user_frame: execution.child_user_frame, service_state: execution.service_state, spawn_observation: execution.spawn_observation, receive_observation: execution.receive_observation, ingress: execution.ingress, composition: execution.composition, failure_observation: execution.failure_observation, wait_observation: execution.wait_observation, ready_queue: execution.ready_queue }
}

func ensure_serial_service_endpoint(config: SerialServiceConfig, execution: SerialServiceExecutionState) SerialServiceExecutionState {
    if ipc.resolve_runtime_endpoint(execution.endpoints, config.serial_endpoint_id).valid != 0 {
        return execution
    }
    return SerialServiceExecutionState{ program_capability: execution.program_capability, gate: execution.gate, process_slots: execution.process_slots, task_slots: execution.task_slots, init_handle_table: execution.init_handle_table, child_handle_table: execution.child_handle_table, wait_table: execution.wait_table, endpoints: ipc.install_endpoint(execution.endpoints, config.init_pid, config.serial_endpoint_id), init_image: execution.init_image, child_address_space: execution.child_address_space, child_user_frame: execution.child_user_frame, service_state: execution.service_state, spawn_observation: execution.spawn_observation, receive_observation: execution.receive_observation, ingress: execution.ingress, composition: execution.composition, failure_observation: execution.failure_observation, wait_observation: execution.wait_observation, ready_queue: execution.ready_queue }
}

func spawn_serial_service(config: SerialServiceConfig, execution: SerialServiceExecutionState) SerialServiceExecutionResult {
    next_state: SerialServiceExecutionState = ensure_serial_service_endpoint(config, execution)
    child_handles: capability.HandleTable = capability.handle_table_for_owner(config.child_pid)
    wait_table: capability.WaitTable = capability.wait_table_for_owner(config.init_pid)
    spawn_request: syscall.SpawnRequest = syscall.build_spawn_request(config.wait_handle_slot)
    spawn_result: syscall.SpawnResult = syscall.perform_spawn(next_state.gate, next_state.program_capability, next_state.process_slots, next_state.task_slots, wait_table, next_state.init_image, spawn_request, config.child_pid, config.child_tid, config.child_asid, config.child_translation_root)
    child_handles = capability.install_endpoint_handle(child_handles, config.endpoint_handle_slot, config.serial_endpoint_id, 3)
    if config.composition_endpoint_id != 0 {
        child_handles = capability.install_endpoint_handle(child_handles, config.composition_handle_slot, config.composition_endpoint_id, 3)
    }
    next_state = SerialServiceExecutionState{ program_capability: spawn_result.program_capability, gate: spawn_result.gate, process_slots: spawn_result.process_slots, task_slots: spawn_result.task_slots, init_handle_table: next_state.init_handle_table, child_handle_table: child_handles, wait_table: spawn_result.wait_table, endpoints: next_state.endpoints, init_image: next_state.init_image, child_address_space: spawn_result.child_address_space, child_user_frame: spawn_result.child_frame, service_state: serial_service.service_state(config.child_pid, config.endpoint_handle_slot), spawn_observation: spawn_result.observation, receive_observation: next_state.receive_observation, ingress: next_state.ingress, composition: next_state.composition, failure_observation: empty_serial_service_failure_observation(), wait_observation: next_state.wait_observation, ready_queue: state.user_ready_queue(config.child_tid) }
    if syscall.status_score(next_state.spawn_observation.status) != 2 {
        return serial_service_result(next_state, 0)
    }
    if capability.find_endpoint_for_handle(next_state.child_handle_table, config.endpoint_handle_slot) != config.serial_endpoint_id {
        return serial_service_result(next_state, 0)
    }
    return serial_service_result(next_state, 1)
}

func execute_serial_service_receive_and_exit(config: SerialServiceConfig, execution: SerialServiceExecutionState) SerialServiceExecutionResult {
    received: SerialServiceExecutionResult = execute_serial_service_receive(config, execution)
    if received.succeeded == 0 {
        return received
    }
    next_state: SerialServiceExecutionState = simulate_serial_service_exit(config, received.state)
    wait_result: syscall.WaitResult = syscall.perform_wait(next_state.gate, next_state.process_slots, next_state.task_slots, next_state.wait_table, syscall.build_wait_request(config.wait_handle_slot))
    next_state = SerialServiceExecutionState{ program_capability: next_state.program_capability, gate: wait_result.gate, process_slots: wait_result.process_slots, task_slots: wait_result.task_slots, init_handle_table: next_state.init_handle_table, child_handle_table: capability.empty_handle_table(), wait_table: wait_result.wait_table, endpoints: next_state.endpoints, init_image: next_state.init_image, child_address_space: next_state.child_address_space, child_user_frame: next_state.child_user_frame, service_state: next_state.service_state, spawn_observation: next_state.spawn_observation, receive_observation: next_state.receive_observation, ingress: next_state.ingress, composition: next_state.composition, failure_observation: next_state.failure_observation, wait_observation: wait_result.observation, ready_queue: state.empty_queue() }
    if syscall.status_score(next_state.wait_observation.status) != 2 {
        return serial_service_result(next_state, 0)
    }
    return serial_service_result(next_state, 1)
}

func execute_serial_service_receive(config: SerialServiceConfig, execution: SerialServiceExecutionState) SerialServiceExecutionResult {
    service_receive: syscall.ReceiveResult = syscall.perform_receive(execution.gate, execution.child_handle_table, execution.endpoints, syscall.build_receive_request(config.endpoint_handle_slot))
    updated_service_state: serial_service.SerialServiceState = serial_service.record_ingress(execution.service_state, service_receive.observation)
    next_state: SerialServiceExecutionState = SerialServiceExecutionState{ program_capability: execution.program_capability, gate: service_receive.gate, process_slots: execution.process_slots, task_slots: execution.task_slots, init_handle_table: execution.init_handle_table, child_handle_table: service_receive.handle_table, wait_table: execution.wait_table, endpoints: service_receive.endpoints, init_image: execution.init_image, child_address_space: execution.child_address_space, child_user_frame: execution.child_user_frame, service_state: updated_service_state, spawn_observation: execution.spawn_observation, receive_observation: service_receive.observation, ingress: serial_service.observe_ingress(updated_service_state), composition: serial_service.observe_composition(updated_service_state), failure_observation: execution.failure_observation, wait_observation: execution.wait_observation, ready_queue: execution.ready_queue }
    if syscall.status_score(next_state.receive_observation.status) != 2 {
        return serial_service_result(next_state, 0)
    }
    return serial_service_result(next_state, 1)
}

func simulate_serial_service_exit(config: SerialServiceConfig, execution: SerialServiceExecutionState) SerialServiceExecutionState {
    processes: [3]state.ProcessSlot = lifecycle.exit_process(execution.process_slots, 2)
    tasks: [3]state.TaskSlot = lifecycle.exit_task(execution.task_slots, 2)
    wait_table: capability.WaitTable = capability.mark_wait_handle_exited(execution.wait_table, config.child_pid, config.exit_code)
    return SerialServiceExecutionState{ program_capability: execution.program_capability, gate: execution.gate, process_slots: processes, task_slots: tasks, init_handle_table: execution.init_handle_table, child_handle_table: execution.child_handle_table, wait_table: wait_table, endpoints: execution.endpoints, init_image: execution.init_image, child_address_space: execution.child_address_space, child_user_frame: execution.child_user_frame, service_state: execution.service_state, spawn_observation: execution.spawn_observation, receive_observation: execution.receive_observation, ingress: execution.ingress, composition: execution.composition, failure_observation: execution.failure_observation, wait_observation: execution.wait_observation, ready_queue: state.empty_queue() }
}

func close_serial_service_after_failure(config: SerialServiceConfig, execution: SerialServiceExecutionState) SerialServiceExecutionResult {
    next_state: SerialServiceExecutionState = simulate_serial_service_exit(config, execution)
    close_result: ipc.EndpointCloseTransition = ipc.close_runtime_endpoint(next_state.endpoints, config.serial_endpoint_id)
    next_state = SerialServiceExecutionState{ program_capability: next_state.program_capability, gate: next_state.gate, process_slots: next_state.process_slots, task_slots: next_state.task_slots, init_handle_table: next_state.init_handle_table, child_handle_table: next_state.child_handle_table, wait_table: next_state.wait_table, endpoints: close_result.endpoints, init_image: next_state.init_image, child_address_space: next_state.child_address_space, child_user_frame: next_state.child_user_frame, service_state: next_state.service_state, spawn_observation: next_state.spawn_observation, receive_observation: next_state.receive_observation, ingress: next_state.ingress, composition: next_state.composition, failure_observation: SerialServiceFailureObservation{ endpoint_id: config.serial_endpoint_id, closed: close_result.closed, aborted_messages: close_result.aborted_messages, wake_count: close_result.wakes.count }, wait_observation: next_state.wait_observation, ready_queue: state.empty_queue() }
    wait_result: syscall.WaitResult = syscall.perform_wait(next_state.gate, next_state.process_slots, next_state.task_slots, next_state.wait_table, syscall.build_wait_request(config.wait_handle_slot))
    next_state = SerialServiceExecutionState{ program_capability: next_state.program_capability, gate: wait_result.gate, process_slots: wait_result.process_slots, task_slots: wait_result.task_slots, init_handle_table: next_state.init_handle_table, child_handle_table: capability.empty_handle_table(), wait_table: wait_result.wait_table, endpoints: next_state.endpoints, init_image: next_state.init_image, child_address_space: next_state.child_address_space, child_user_frame: next_state.child_user_frame, service_state: next_state.service_state, spawn_observation: next_state.spawn_observation, receive_observation: next_state.receive_observation, ingress: next_state.ingress, composition: next_state.composition, failure_observation: next_state.failure_observation, wait_observation: wait_result.observation, ready_queue: state.empty_queue() }
    if next_state.failure_observation.closed == 0 {
        return serial_service_result(next_state, 0)
    }
    if syscall.status_score(next_state.wait_observation.status) != 2 {
        return serial_service_result(next_state, 0)
    }
    return serial_service_result(next_state, 1)
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

func execute_phase140_serial_ingress_composed_service_graph(config: SerialServiceConfig, composition_config: CompositionServiceConfig, serial_execution: SerialServiceExecutionState, composition_execution: CompositionServiceExecutionState) Phase140SerialIngressCompositionResult {
    next_serial: SerialServiceExecutionState = serial_execution
    request_len: usize = serial_service.forwarded_request_len(next_serial.receive_observation.payload_len)
    request_payload: [4]u8 = serial_service.forwarded_request_payload(next_serial.receive_observation.payload_len, next_serial.receive_observation.payload)
    if request_len == 0 {
        return phase140_serial_ingress_composition_result(next_serial, composition_execution, 0)
    }

    next_endpoints: ipc.EndpointTable = next_serial.endpoints
    if config.composition_endpoint_id != 0 && ipc.resolve_runtime_endpoint(next_endpoints, config.composition_endpoint_id).valid == 0 {
        next_endpoints = ipc.install_endpoint(next_endpoints, next_serial.service_state.owner_pid, config.composition_endpoint_id)
    }

    serial_handles: capability.HandleTable = next_serial.child_handle_table
    if config.composition_endpoint_id != 0 {
        serial_handles = capability.install_endpoint_handle(serial_handles, config.composition_handle_slot, config.composition_endpoint_id, 3)
    }
    next_serial = SerialServiceExecutionState{ program_capability: next_serial.program_capability, gate: next_serial.gate, process_slots: next_serial.process_slots, task_slots: next_serial.task_slots, init_handle_table: next_serial.init_handle_table, child_handle_table: serial_handles, wait_table: next_serial.wait_table, endpoints: next_endpoints, init_image: next_serial.init_image, child_address_space: next_serial.child_address_space, child_user_frame: next_serial.child_user_frame, service_state: next_serial.service_state, spawn_observation: next_serial.spawn_observation, receive_observation: next_serial.receive_observation, ingress: next_serial.ingress, composition: next_serial.composition, failure_observation: next_serial.failure_observation, wait_observation: next_serial.wait_observation, ready_queue: next_serial.ready_queue }
    if capability.find_endpoint_for_handle(next_serial.child_handle_table, config.composition_handle_slot) != config.composition_endpoint_id {
        return phase140_serial_ingress_composition_result(next_serial, composition_execution, 0)
    }

    forward_send: syscall.SendResult = syscall.perform_send(next_serial.gate, next_serial.child_handle_table, next_serial.endpoints, next_serial.service_state.owner_pid, syscall.build_send_request(config.composition_handle_slot, request_len, request_payload))
    updated_serial_service: serial_service.SerialServiceState = serial_service.record_forward(next_serial.service_state, config.composition_endpoint_id, forward_send.status, request_len, request_payload)
    next_serial = SerialServiceExecutionState{ program_capability: next_serial.program_capability, gate: forward_send.gate, process_slots: next_serial.process_slots, task_slots: next_serial.task_slots, init_handle_table: next_serial.init_handle_table, child_handle_table: forward_send.handle_table, wait_table: next_serial.wait_table, endpoints: forward_send.endpoints, init_image: next_serial.init_image, child_address_space: next_serial.child_address_space, child_user_frame: next_serial.child_user_frame, service_state: updated_serial_service, spawn_observation: next_serial.spawn_observation, receive_observation: next_serial.receive_observation, ingress: next_serial.ingress, composition: serial_service.observe_composition(updated_serial_service), failure_observation: next_serial.failure_observation, wait_observation: next_serial.wait_observation, ready_queue: next_serial.ready_queue }
    if syscall.status_score(next_serial.composition.forward_status) != 2 {
        return phase140_serial_ingress_composition_result(next_serial, composition_execution, 0)
    }

    prepared_composition: CompositionServiceExecutionState = prepare_composition_service_local_execution(composition_config, CompositionServiceExecutionState{ program_capability: composition_execution.program_capability, gate: next_serial.gate, process_slots: composition_execution.process_slots, task_slots: composition_execution.task_slots, init_handle_table: next_serial.child_handle_table, child_handle_table: composition_execution.child_handle_table, wait_table: composition_execution.wait_table, endpoints: next_serial.endpoints, init_image: composition_execution.init_image, child_address_space: composition_execution.child_address_space, child_user_frame: composition_execution.child_user_frame, echo_peer_state: composition_execution.echo_peer_state, log_peer_state: composition_execution.log_peer_state, spawn_observation: composition_execution.spawn_observation, request_receive_observation: composition_execution.request_receive_observation, echo_fanout_observation: composition_execution.echo_fanout_observation, echo_peer_receive_observation: composition_execution.echo_peer_receive_observation, echo_reply_send_observation: composition_execution.echo_reply_send_observation, echo_reply_observation: composition_execution.echo_reply_observation, log_fanout_observation: composition_execution.log_fanout_observation, log_peer_receive_observation: composition_execution.log_peer_receive_observation, log_ack_send_observation: composition_execution.log_ack_send_observation, log_ack_observation: composition_execution.log_ack_observation, aggregate_reply_send_observation: composition_execution.aggregate_reply_send_observation, aggregate_reply_observation: composition_execution.aggregate_reply_observation, wait_observation: composition_execution.wait_observation, observation: composition_execution.observation, ready_queue: composition_execution.ready_queue })
    composed: CompositionServiceExecutionResult = execute_composition_service_queued_request(composition_config, prepared_composition)

    next_serial = SerialServiceExecutionState{ program_capability: next_serial.program_capability, gate: composed.state.gate, process_slots: next_serial.process_slots, task_slots: next_serial.task_slots, init_handle_table: next_serial.init_handle_table, child_handle_table: composed.state.init_handle_table, wait_table: next_serial.wait_table, endpoints: composed.state.endpoints, init_image: next_serial.init_image, child_address_space: next_serial.child_address_space, child_user_frame: next_serial.child_user_frame, service_state: next_serial.service_state, spawn_observation: next_serial.spawn_observation, receive_observation: next_serial.receive_observation, ingress: next_serial.ingress, composition: next_serial.composition, failure_observation: next_serial.failure_observation, wait_observation: next_serial.wait_observation, ready_queue: next_serial.ready_queue }
    if composed.succeeded == 0 {
        return phase140_serial_ingress_composition_result(next_serial, composed.state, 0)
    }

    updated_serial_service = serial_service.record_aggregate_reply(next_serial.service_state, composed.state.aggregate_reply_observation)
    next_serial = SerialServiceExecutionState{ program_capability: next_serial.program_capability, gate: next_serial.gate, process_slots: next_serial.process_slots, task_slots: next_serial.task_slots, init_handle_table: next_serial.init_handle_table, child_handle_table: next_serial.child_handle_table, wait_table: next_serial.wait_table, endpoints: next_serial.endpoints, init_image: next_serial.init_image, child_address_space: next_serial.child_address_space, child_user_frame: next_serial.child_user_frame, service_state: updated_serial_service, spawn_observation: next_serial.spawn_observation, receive_observation: next_serial.receive_observation, ingress: next_serial.ingress, composition: serial_service.observe_composition(updated_serial_service), failure_observation: next_serial.failure_observation, wait_observation: next_serial.wait_observation, ready_queue: next_serial.ready_queue }
    if syscall.status_score(next_serial.composition.aggregate_reply_status) != 2 {
        return phase140_serial_ingress_composition_result(next_serial, composed.state, 0)
    }
    return phase140_serial_ingress_composition_result(next_serial, composed.state, 1)
}
