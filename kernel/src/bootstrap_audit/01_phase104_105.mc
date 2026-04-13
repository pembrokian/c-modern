func validate_timer_hardening_contracts() bool {
    premature_timer: timer.TimerState = timer.empty_timer_state()
    premature_timer = timer.arm_sleep(premature_timer, 41, 5)
    premature_consumed: timer.TimerState = timer.consume_wake(premature_timer, 41)
    if premature_consumed.count != 1 {
        return false
    }
    if timer.find_sleep_index(premature_consumed, 41) != 0 {
        return false
    }
    if timer.sleep_state_score(premature_consumed.sleepers[0].state) != 2 {
        return false
    }

    dual_timer: timer.TimerState = timer.empty_timer_state()
    dual_timer = timer.arm_sleep(dual_timer, 51, 1)
    dual_timer = timer.arm_sleep(dual_timer, 52, 1)
    dual_timer = timer.advance_tick(dual_timer, 1)
    if timer.has_fired_sleeper(dual_timer) == 0 {
        return false
    }
    first_wake: timer.TimerWakeResult = timer.wake_fired_sleepers(dual_timer)
    if first_wake.observation.task_id != 51 {
        return false
    }
    dual_timer = timer.consume_wake(first_wake.timer_state, first_wake.observation.task_id)
    if dual_timer.count != 1 {
        return false
    }
    if timer.has_fired_sleeper(dual_timer) == 0 {
        return false
    }
    second_wake: timer.TimerWakeResult = timer.wake_fired_sleepers(dual_timer)
    if second_wake.observation.task_id != 52 {
        return false
    }
    if second_wake.observation.wake_count != 2 {
        return false
    }
    dual_timer = timer.consume_wake(second_wake.timer_state, second_wake.observation.task_id)
    if dual_timer.count != 0 {
        return false
    }
    return timer.has_fired_sleeper(dual_timer) == 0
}

func validate_bootstrap_layout_contracts(audit: BootstrapLayoutAudit) bool {
    if !init.bootstrap_image_valid(audit.init_image) {
        return false
    }
    invalid_image: init.InitImage = init.InitImage{ image_id: 9, image_base: 65536, image_size: 4096, entry_pc: 70000, stack_base: 67584, stack_top: 71680, stack_size: 4096 }
    if init.bootstrap_image_valid(invalid_image) {
        return false
    }
    invalid_space: address_space.AddressSpace = address_space.bootstrap_space(3, 7, mmu.bootstrap_translation_root(3, audit.init_root_page_table), invalid_image.image_base, invalid_image.image_size, invalid_image.entry_pc, invalid_image.stack_base, invalid_image.stack_size, invalid_image.stack_top)
    if address_space.state_score(invalid_space.state) != 1 {
        return false
    }
    if address_space.can_activate(address_space.empty_space()) {
        return false
    }
    still_empty: address_space.AddressSpace = address_space.activate(address_space.empty_space())
    return address_space.state_score(still_empty.state) == 1
}

func validate_endpoint_and_capability_contracts(audit: EndpointCapabilityAudit) bool {
    payload: [4]u8 = ipc.zero_payload()
    local_endpoints: ipc.EndpointTable = ipc.empty_table()
    local_endpoints = ipc.install_endpoint(local_endpoints, audit.init_pid, audit.init_endpoint_id)
    enqueue_one: ipc.EndpointTable = ipc.enqueue_message(local_endpoints, 0, ipc.byte_message(2, audit.init_pid, audit.init_endpoint_id, 0, payload))
    if !ipc.enqueue_succeeded(local_endpoints, enqueue_one, 0) {
        return false
    }
    enqueue_two: ipc.EndpointTable = ipc.enqueue_message(enqueue_one, 0, ipc.byte_message(3, audit.init_pid, audit.init_endpoint_id, 0, payload))
    if !ipc.enqueue_succeeded(enqueue_one, enqueue_two, 0) {
        return false
    }
    enqueue_three: ipc.EndpointTable = ipc.enqueue_message(enqueue_two, 0, ipc.byte_message(4, audit.init_pid, audit.init_endpoint_id, 0, payload))
    if ipc.enqueue_succeeded(enqueue_two, enqueue_three, 0) {
        return false
    }

    local_handles: capability.HandleTable = capability.handle_table_for_owner(audit.init_pid)
    installed_handle: capability.HandleTable = capability.install_endpoint_handle(local_handles, 1, audit.init_endpoint_id, 7)
    if !capability.handle_install_succeeded(local_handles, installed_handle, 1) {
        return false
    }
    duplicate_handle: capability.HandleTable = capability.install_endpoint_handle(installed_handle, 1, audit.transfer_endpoint_id, 7)
    if capability.handle_install_succeeded(installed_handle, duplicate_handle, 1) {
        return false
    }
    invalid_rights_handle: capability.HandleTable = capability.install_endpoint_handle(local_handles, 2, audit.transfer_endpoint_id, 8)
    if capability.handle_install_succeeded(local_handles, invalid_rights_handle, 2) {
        return false
    }
    if capability.attenuate_endpoint_rights(8) != 0 {
        return false
    }
    if capability.find_transfer_rights_for_handle(installed_handle, 1) != 7 {
        return false
    }

    local_waits: capability.WaitTable = capability.wait_table_for_owner(audit.init_pid)
    installed_wait: capability.WaitTable = capability.install_wait_handle(local_waits, audit.child_wait_handle_slot, 3)
    if !capability.wait_install_succeeded(local_waits, installed_wait, audit.child_wait_handle_slot) {
        return false
    }
    duplicate_wait: capability.WaitTable = capability.install_wait_handle(installed_wait, audit.child_wait_handle_slot, 3)
    return !capability.wait_install_succeeded(installed_wait, duplicate_wait, audit.child_wait_handle_slot)
}

func validate_state_hardening_contracts(audit: StateHardeningAudit) bool {
    boot_task: state.TaskSlot = state.boot_task_slot(audit.boot_tid, audit.boot_pid, audit.boot_entry_pc, audit.boot_stack_top)
    if state.can_block_task(boot_task) {
        return false
    }
    if state.task_state_score(state.blocked_task_slot(boot_task).state) != 2 {
        return false
    }
    ready_task: state.TaskSlot = state.user_task_slot(audit.child_tid, audit.child_pid, audit.child_asid, audit.init_image.entry_pc, audit.init_image.stack_top)
    if !state.can_block_task(ready_task) {
        return false
    }
    if state.task_state_score(state.blocked_task_slot(ready_task).state) != 8 {
        return false
    }

    local_log: state.BootLog = state.empty_log()
    append0: state.BootLogAppendResult = state.append_record(local_log, state.BootStage.Reset, audit.arch_actor, 1)
    append1: state.BootLogAppendResult = state.append_record(append0.log, state.BootStage.TablesSeeded, audit.arch_actor, 2)
    append2: state.BootLogAppendResult = state.append_record(append1.log, state.BootStage.AddressSpaceReady, audit.arch_actor, 3)
    append3: state.BootLogAppendResult = state.append_record(append2.log, state.BootStage.UserEntryReady, audit.arch_actor, 4)
    append4: state.BootLogAppendResult = state.append_record(append3.log, state.BootStage.MarkerEmitted, audit.arch_actor, 5)
    append5: state.BootLogAppendResult = state.append_record(append4.log, state.BootStage.Halted, audit.arch_actor, 6)
    append6: state.BootLogAppendResult = state.append_record(append5.log, state.BootStage.Halted, audit.arch_actor, 7)
    append7: state.BootLogAppendResult = state.append_record(append6.log, state.BootStage.Halted, audit.arch_actor, 8)
    append8: state.BootLogAppendResult = state.append_record(append7.log, state.BootStage.Halted, audit.arch_actor, 9)
    append9: state.BootLogAppendResult = state.append_record(append8.log, state.BootStage.Halted, audit.arch_actor, 10)
    append10: state.BootLogAppendResult = state.append_record(append9.log, state.BootStage.Halted, audit.arch_actor, 11)
    append11: state.BootLogAppendResult = state.append_record(append10.log, state.BootStage.Halted, audit.arch_actor, 12)
    append12: state.BootLogAppendResult = state.append_record(append11.log, state.BootStage.Halted, audit.arch_actor, 13)
    append13: state.BootLogAppendResult = state.append_record(append12.log, state.BootStage.Halted, audit.arch_actor, 14)
    append14: state.BootLogAppendResult = state.append_record(append13.log, state.BootStage.Halted, audit.arch_actor, 15)
    overflow: state.BootLogAppendResult = state.append_record(append14.log, state.BootStage.Halted, audit.arch_actor, 16)
    if overflow.appended != 0 {
        return false
    }
    return overflow.log.count == 15
}

func validate_syscall_contract_hardening(audit: SyscallHardeningAudit) bool {
    local_gate: syscall.SyscallGate = syscall.open_gate(syscall.gate_closed())
    payload: [4]u8 = ipc.zero_payload()
    payload[0] = 81
    payload[1] = 85
    payload[2] = 69
    payload[3] = 85

    local_handles: capability.HandleTable = capability.handle_table_for_owner(audit.init_pid)
    local_handles = capability.install_endpoint_handle(local_handles, audit.init_endpoint_handle_slot, audit.init_endpoint_id, 3)
    local_endpoints: ipc.EndpointTable = ipc.empty_table()
    local_endpoints = ipc.install_endpoint(local_endpoints, audit.init_pid, audit.init_endpoint_id)

    send_one: syscall.SendResult = syscall.perform_send(local_gate, local_handles, local_endpoints, audit.init_pid, syscall.build_send_request(audit.init_endpoint_handle_slot, 4, payload))
    send_two: syscall.SendResult = syscall.perform_send(send_one.gate, send_one.handle_table, send_one.endpoints, audit.init_pid, syscall.build_send_request(audit.init_endpoint_handle_slot, 4, payload))
    send_block: syscall.SendResult = syscall.perform_send(send_two.gate, send_two.handle_table, send_two.endpoints, audit.init_pid, syscall.build_send_request(audit.init_endpoint_handle_slot, 4, payload))
    if syscall.status_score(send_block.status) != 4 {
        return false
    }
    if syscall.block_reason_score(send_block.block_reason) != 2 {
        return false
    }
    if send_block.endpoints.slots[0].queued_messages != 2 {
        return false
    }

    receive_one: syscall.ReceiveResult = syscall.perform_receive(send_block.gate, send_block.handle_table, send_block.endpoints, syscall.build_receive_request(audit.init_endpoint_handle_slot))
    receive_two: syscall.ReceiveResult = syscall.perform_receive(receive_one.gate, receive_one.handle_table, receive_one.endpoints, syscall.build_receive_request(audit.init_endpoint_handle_slot))
    receive_block: syscall.ReceiveResult = syscall.perform_receive(receive_two.gate, receive_two.handle_table, receive_two.endpoints, syscall.build_receive_request(audit.init_endpoint_handle_slot))
    if syscall.status_score(receive_block.observation.status) != 4 {
        return false
    }
    if syscall.block_reason_score(receive_block.observation.block_reason) != 4 {
        return false
    }

    local_waits: capability.WaitTable = capability.wait_table_for_owner(audit.init_pid)
    local_waits = capability.install_wait_handle(local_waits, audit.child_wait_handle_slot, audit.child_pid)
    wait_block: syscall.WaitResult = syscall.perform_wait(local_gate, state.zero_process_slots(), state.zero_task_slots(), local_waits, syscall.build_wait_request(audit.child_wait_handle_slot))
    if syscall.status_score(wait_block.observation.status) != 4 {
        return false
    }
    if syscall.block_reason_score(wait_block.observation.block_reason) != 8 {
        return false
    }

    sleep_tasks: [3]state.TaskSlot = state.zero_task_slots()
    sleep_tasks[1] = state.user_task_slot(audit.child_tid, audit.child_pid, audit.child_asid, audit.init_image.entry_pc, audit.init_image.stack_top)
    sleep_block: syscall.SleepResult = syscall.perform_sleep(local_gate, sleep_tasks, timer.empty_timer_state(), syscall.build_sleep_request(1, 2))
    if syscall.status_score(sleep_block.observation.status) != 4 {
        return false
    }
    if syscall.block_reason_score(sleep_block.observation.block_reason) != 16 {
        return false
    }

    spawn_process_slots: [3]state.ProcessSlot = state.zero_process_slots()
    spawn_process_slots[0] = state.boot_process_slot(audit.boot_pid, audit.boot_task_slot)
    spawn_process_slots[2] = state.init_process_slot(9, 2, 9)
    spawn_task_slots: [3]state.TaskSlot = state.zero_task_slots()
    spawn_task_slots[0] = state.boot_task_slot(audit.boot_tid, audit.boot_pid, audit.boot_entry_pc, audit.boot_stack_top)
    spawn_task_slots[2] = state.user_task_slot(9, 9, 9, audit.init_image.entry_pc, audit.init_image.stack_top)
    spawn_waits: capability.WaitTable = capability.wait_table_for_owner(audit.init_pid)
    local_program_capability: capability.CapabilitySlot = capability.bootstrap_init_program_slot(audit.init_pid)
    local_spawn: syscall.SpawnResult = syscall.perform_spawn(local_gate, local_program_capability, spawn_process_slots, spawn_task_slots, spawn_waits, audit.init_image, syscall.build_spawn_request(audit.child_wait_handle_slot), audit.child_pid, audit.child_tid, audit.child_asid, mmu.bootstrap_translation_root(audit.child_asid, audit.child_root_page_table))
    if syscall.status_score(local_spawn.observation.status) != 2 {
        return false
    }
    if local_spawn.process_slots[1].pid != audit.child_pid {
        return false
    }
    if local_spawn.process_slots[2].pid != 9 {
        return false
    }
    if local_spawn.task_slots[1].tid != audit.child_tid {
        return false
    }
    if local_spawn.task_slots[2].tid != 9 {
        return false
    }
    if !capability.wait_handle_installed(local_spawn.wait_table, audit.child_wait_handle_slot) {
        return false
    }

    transfer_handles: capability.HandleTable = capability.handle_table_for_owner(audit.init_pid)
    transfer_handles = capability.install_endpoint_handle(transfer_handles, audit.init_endpoint_handle_slot, audit.init_endpoint_id, 3)
    transfer_handles = capability.install_endpoint_handle(transfer_handles, audit.transfer_source_handle_slot, 2, 5)
    transfer_endpoints: ipc.EndpointTable = ipc.empty_table()
    transfer_endpoints = ipc.install_endpoint(transfer_endpoints, audit.init_pid, audit.init_endpoint_id)
    transfer_send: syscall.SendResult = syscall.perform_send(local_gate, transfer_handles, transfer_endpoints, audit.init_pid, syscall.build_transfer_send_request(audit.init_endpoint_handle_slot, 4, payload, audit.transfer_source_handle_slot))
    if capability.find_endpoint_for_handle(transfer_send.handle_table, audit.transfer_source_handle_slot) != 0 {
        return false
    }
    blocked_receive: syscall.ReceiveResult = syscall.perform_receive(transfer_send.gate, transfer_send.handle_table, transfer_send.endpoints, syscall.build_transfer_receive_request(audit.init_endpoint_handle_slot, audit.init_endpoint_handle_slot))
    if syscall.status_score(blocked_receive.observation.status) != 8 {
        return false
    }
    if blocked_receive.endpoints.slots[0].queued_messages != 1 {
        return false
    }
    return capability.find_endpoint_for_handle(blocked_receive.handle_table, audit.transfer_source_handle_slot) == 0
}

func validate_phase104_contract_hardening(audit: Phase104HardeningAudit) bool {
    if audit.boot_log_append_failed != 0 {
        return false
    }
    if audit.timer_hardened == 0 {
        return false
    }
    if !validate_bootstrap_layout_contracts(audit.bootstrap_layout) {
        return false
    }
    if !validate_endpoint_and_capability_contracts(audit.endpoint_capability) {
        return false
    }
    if !validate_state_hardening_contracts(audit.state_hardening) {
        return false
    }
    return validate_syscall_contract_hardening(audit.syscall_hardening)
}

func validate_phase105_log_service_handshake(audit: LogServicePhaseAudit) bool {
    if capability.kind_score(audit.program_capability.kind) != 1 {
        return false
    }
    if syscall.id_score(audit.gate.last_id) != 16 {
        return false
    }
    if syscall.status_score(audit.gate.last_status) != 2 {
        return false
    }
    if audit.gate.send_count != 5 {
        return false
    }
    if audit.gate.receive_count != 5 {
        return false
    }
    if audit.spawn_observation.child_pid != audit.child_pid {
        return false
    }
    if audit.spawn_observation.child_tid != audit.child_tid {
        return false
    }
    if audit.spawn_observation.child_asid != audit.child_asid {
        return false
    }
    if audit.spawn_observation.wait_handle_slot != audit.wait_handle_slot {
        return false
    }
    if syscall.status_score(audit.spawn_observation.status) != 2 {
        return false
    }
    if audit.receive_observation.endpoint_id != audit.init_endpoint_id {
        return false
    }
    if audit.receive_observation.source_pid != audit.init_pid {
        return false
    }
    if audit.receive_observation.payload_len != 1 {
        return false
    }
    if audit.receive_observation.payload[0] != audit.request_byte {
        return false
    }
    if audit.ack_observation.endpoint_id != audit.init_endpoint_id {
        return false
    }
    if audit.ack_observation.source_pid != audit.child_pid {
        return false
    }
    if audit.ack_observation.payload_len != 1 {
        return false
    }
    if audit.ack_observation.payload[0] != 33 {
        return false
    }
    if audit.service_state.owner_pid != audit.child_pid {
        return false
    }
    if audit.service_state.endpoint_handle_slot != audit.endpoint_handle_slot {
        return false
    }
    if audit.service_state.handled_request_count != 1 {
        return false
    }
    if audit.service_state.ack_count != 1 {
        return false
    }
    if audit.service_state.last_client_pid != audit.init_pid {
        return false
    }
    if audit.service_state.last_endpoint_id != audit.init_endpoint_id {
        return false
    }
    if audit.service_state.last_request_len != 1 {
        return false
    }
    if audit.service_state.last_request_byte != audit.request_byte {
        return false
    }
    if audit.service_state.last_ack_byte != 33 {
        return false
    }
    if audit.handshake.service_pid != audit.child_pid {
        return false
    }
    if audit.handshake.client_pid != audit.init_pid {
        return false
    }
    if audit.handshake.endpoint_id != audit.init_endpoint_id {
        return false
    }
    if log_service.tag_score(audit.handshake.tag) != 16 {
        return false
    }
    if audit.handshake.request_len != 1 {
        return false
    }
    if audit.handshake.request_byte != audit.request_byte {
        return false
    }
    if audit.handshake.ack_byte != 33 {
        return false
    }
    if audit.handshake.request_count != 1 {
        return false
    }
    if audit.handshake.ack_count != 1 {
        return false
    }
    if syscall.status_score(audit.wait_observation.status) != 2 {
        return false
    }
    if audit.wait_observation.child_pid != audit.child_pid {
        return false
    }
    if audit.wait_observation.exit_code != audit.exit_code {
        return false
    }
    if audit.wait_observation.wait_handle_slot != audit.wait_handle_slot {
        return false
    }
    if audit.wait_table.count != 0 {
        return false
    }
    if capability.find_child_for_wait_handle(audit.wait_table, audit.wait_handle_slot) != 0 {
        return false
    }
    if audit.child_handle_table.count != 0 {
        return false
    }
    if audit.ready_queue.count != 0 {
        return false
    }
    if state.process_state_score(audit.child_process.state) != 1 {
        return false
    }
    if state.task_state_score(audit.child_task.state) != 1 {
        return false
    }
    if address_space.state_score(audit.child_address_space.state) != 2 {
        return false
    }
    if audit.child_address_space.owner_pid != audit.child_pid {
        return false
    }
    return audit.child_user_frame.task_id == audit.child_tid
}

