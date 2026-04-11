func validate_phase106_echo_service_request_reply(audit: EchoServicePhaseAudit) bool {
    if capability.kind_score(audit.program_capability.kind) != 1 {
        return false
    }
    if syscall.id_score(audit.gate.last_id) != 16 {
        return false
    }
    if syscall.status_score(audit.gate.last_status) != 2 {
        return false
    }
    if audit.gate.send_count != 7 {
        return false
    }
    if audit.gate.receive_count != 7 {
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
    if audit.receive_observation.payload_len != 2 {
        return false
    }
    if audit.receive_observation.payload[0] != audit.request_byte0 {
        return false
    }
    if audit.receive_observation.payload[1] != audit.request_byte1 {
        return false
    }
    if audit.reply_observation.endpoint_id != audit.init_endpoint_id {
        return false
    }
    if audit.reply_observation.source_pid != audit.child_pid {
        return false
    }
    if audit.reply_observation.payload_len != 2 {
        return false
    }
    if audit.reply_observation.payload[0] != audit.request_byte0 {
        return false
    }
    if audit.reply_observation.payload[1] != audit.request_byte1 {
        return false
    }
    if audit.service_state.owner_pid != audit.child_pid {
        return false
    }
    if audit.service_state.endpoint_handle_slot != audit.endpoint_handle_slot {
        return false
    }
    if audit.service_state.request_count != 1 {
        return false
    }
    if audit.service_state.reply_count != 1 {
        return false
    }
    if audit.service_state.last_client_pid != audit.init_pid {
        return false
    }
    if audit.service_state.last_endpoint_id != audit.init_endpoint_id {
        return false
    }
    if audit.service_state.last_request_len != 2 {
        return false
    }
    if audit.service_state.last_request_byte0 != audit.request_byte0 {
        return false
    }
    if audit.service_state.last_request_byte1 != audit.request_byte1 {
        return false
    }
    if audit.service_state.last_reply_len != 2 {
        return false
    }
    if audit.service_state.last_reply_byte0 != audit.request_byte0 {
        return false
    }
    if audit.service_state.last_reply_byte1 != audit.request_byte1 {
        return false
    }
    if audit.exchange.service_pid != audit.child_pid {
        return false
    }
    if audit.exchange.client_pid != audit.init_pid {
        return false
    }
    if audit.exchange.endpoint_id != audit.init_endpoint_id {
        return false
    }
    if echo_service.tag_score(audit.exchange.tag) != 4 {
        return false
    }
    if audit.exchange.request_len != 2 {
        return false
    }
    if audit.exchange.request_byte0 != audit.request_byte0 {
        return false
    }
    if audit.exchange.request_byte1 != audit.request_byte1 {
        return false
    }
    if audit.exchange.reply_len != 2 {
        return false
    }
    if audit.exchange.reply_byte0 != audit.request_byte0 {
        return false
    }
    if audit.exchange.reply_byte1 != audit.request_byte1 {
        return false
    }
    if audit.exchange.request_count != 1 {
        return false
    }
    if audit.exchange.reply_count != 1 {
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

func validate_phase107_user_to_user_capability_transfer(audit: TransferServicePhaseAudit) bool {
    if capability.kind_score(audit.program_capability.kind) != 1 {
        return false
    }
    if syscall.id_score(audit.gate.last_id) != 16 {
        return false
    }
    if syscall.status_score(audit.gate.last_status) != 2 {
        return false
    }
    if audit.gate.send_count != 9 {
        return false
    }
    if audit.gate.receive_count != 9 {
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
    if capability.find_endpoint_for_handle(audit.init_handle_table, audit.source_handle_slot) != 0 {
        return false
    }
    if capability.find_endpoint_for_handle(audit.init_handle_table, audit.init_received_handle_slot) != audit.transfer_endpoint_id {
        return false
    }
    if capability.find_rights_for_handle(audit.init_handle_table, audit.init_received_handle_slot) != 7 {
        return false
    }
    if audit.grant_observation.endpoint_id != audit.init_endpoint_id {
        return false
    }
    if audit.grant_observation.source_pid != audit.init_pid {
        return false
    }
    if audit.grant_observation.payload_len != 4 {
        return false
    }
    if audit.grant_observation.received_handle_slot != audit.service_received_handle_slot {
        return false
    }
    if audit.grant_observation.received_handle_count != 1 {
        return false
    }
    if audit.grant_observation.payload[0] != audit.grant_byte0 {
        return false
    }
    if audit.grant_observation.payload[1] != audit.grant_byte1 {
        return false
    }
    if audit.grant_observation.payload[2] != audit.grant_byte2 {
        return false
    }
    if audit.grant_observation.payload[3] != audit.grant_byte3 {
        return false
    }
    if audit.emit_observation.endpoint_id != audit.transfer_endpoint_id {
        return false
    }
    if audit.emit_observation.source_pid != audit.child_pid {
        return false
    }
    if audit.emit_observation.payload_len != 4 {
        return false
    }
    if audit.emit_observation.received_handle_slot != 0 {
        return false
    }
    if audit.emit_observation.received_handle_count != 0 {
        return false
    }
    if audit.emit_observation.payload[0] != audit.grant_byte0 {
        return false
    }
    if audit.emit_observation.payload[1] != audit.grant_byte1 {
        return false
    }
    if audit.emit_observation.payload[2] != audit.grant_byte2 {
        return false
    }
    if audit.emit_observation.payload[3] != audit.grant_byte3 {
        return false
    }
    if audit.service_state.owner_pid != audit.child_pid {
        return false
    }
    if audit.service_state.control_handle_slot != audit.control_handle_slot {
        return false
    }
    if audit.service_state.transferred_handle_slot != audit.service_received_handle_slot {
        return false
    }
    if audit.service_state.grant_count != 1 {
        return false
    }
    if audit.service_state.emit_count != 1 {
        return false
    }
    if audit.service_state.last_client_pid != audit.init_pid {
        return false
    }
    if audit.service_state.last_control_endpoint_id != audit.init_endpoint_id {
        return false
    }
    if audit.service_state.last_transferred_endpoint_id != audit.transfer_endpoint_id {
        return false
    }
    if audit.service_state.last_transferred_rights != 7 {
        return false
    }
    if audit.service_state.last_grant_len != 4 {
        return false
    }
    if audit.service_state.last_grant_byte0 != audit.grant_byte0 {
        return false
    }
    if audit.service_state.last_grant_byte1 != audit.grant_byte1 {
        return false
    }
    if audit.service_state.last_grant_byte2 != audit.grant_byte2 {
        return false
    }
    if audit.service_state.last_grant_byte3 != audit.grant_byte3 {
        return false
    }
    if audit.service_state.last_emit_len != 4 {
        return false
    }
    if audit.service_state.last_emit_byte0 != audit.grant_byte0 {
        return false
    }
    if audit.service_state.last_emit_byte1 != audit.grant_byte1 {
        return false
    }
    if audit.service_state.last_emit_byte2 != audit.grant_byte2 {
        return false
    }
    if audit.service_state.last_emit_byte3 != audit.grant_byte3 {
        return false
    }
    if audit.transfer.service_pid != audit.child_pid {
        return false
    }
    if audit.transfer.client_pid != audit.init_pid {
        return false
    }
    if audit.transfer.control_endpoint_id != audit.init_endpoint_id {
        return false
    }
    if audit.transfer.transferred_endpoint_id != audit.transfer_endpoint_id {
        return false
    }
    if audit.transfer.transferred_rights != 7 {
        return false
    }
    if transfer_service.tag_score(audit.transfer.tag) != 4 {
        return false
    }
    if audit.transfer.grant_len != 4 {
        return false
    }
    if audit.transfer.grant_byte0 != audit.grant_byte0 {
        return false
    }
    if audit.transfer.grant_byte1 != audit.grant_byte1 {
        return false
    }
    if audit.transfer.grant_byte2 != audit.grant_byte2 {
        return false
    }
    if audit.transfer.grant_byte3 != audit.grant_byte3 {
        return false
    }
    if audit.transfer.emit_len != 4 {
        return false
    }
    if audit.transfer.emit_byte0 != audit.grant_byte0 {
        return false
    }
    if audit.transfer.emit_byte1 != audit.grant_byte1 {
        return false
    }
    if audit.transfer.emit_byte2 != audit.grant_byte2 {
        return false
    }
    if audit.transfer.emit_byte3 != audit.grant_byte3 {
        return false
    }
    if audit.transfer.grant_count != 1 {
        return false
    }
    if audit.transfer.emit_count != 1 {
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

