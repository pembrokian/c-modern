func endpoint_rights_valid(rights: u32) bool {
    if rights == 0 {
        return false
    }
    if rights > RIGHTS_ENDPOINT_ALL {
        return false
    }
    return true
}

func endpoint_rights_include(rights: u32, required_rights: u32) bool {
    if !endpoint_rights_valid(rights) {
        return false
    }
    if required_rights == 0 {
        return true
    }
    if required_rights == RIGHTS_ENDPOINT_SEND {
        return rights == 1 || rights == 3 || rights == 5 || rights == 7
    }
    if required_rights == RIGHTS_ENDPOINT_RECEIVE {
        return rights == 2 || rights == 3 || rights == 6 || rights == 7
    }
    if required_rights == RIGHTS_ENDPOINT_TRANSFER {
        return rights == 4 || rights == 5 || rights == 6 || rights == 7
    }
    if required_rights == 3 {
        return rights == 3 || rights == 7
    }
    if required_rights == 5 {
        return rights == 5 || rights == 7
    }
    if required_rights == 6 {
        return rights == 6 || rights == 7
    }
    if required_rights == RIGHTS_ENDPOINT_ALL {
        return rights == RIGHTS_ENDPOINT_ALL
    }
    return false
}

func handle_slot_accepts_install(state: HandleState) bool {
    return handle_state_score(state) != 2
}

func attenuate_endpoint_rights(rights: u32) u32 {
    if !endpoint_rights_valid(rights) {
        return 0
    }
    return rights
}

func find_transfer_rights_for_handle(table: HandleTable, slot_id: u32) u32 {
    return attenuate_endpoint_rights(find_rights_for_handle(table, slot_id))
}

func install_endpoint_handle(table: HandleTable, slot_id: u32, endpoint_id: u32, rights: u32) HandleTable {
    if table.count >= HANDLE_TABLE_CAPACITY {
        return table
    }
    if !endpoint_rights_valid(rights) {
        return table
    }
    if handle_slot_installed(table, slot_id) {
        return table
    }
    slots: [5]HandleSlot = table.slots
    if handle_slot_accepts_install(slots[0].state) {
        slots[0] = HandleSlot{ slot_id: slot_id, owner_pid: table.owner_pid, endpoint_id: endpoint_id, rights: rights, state: HandleState.Installed }
        return HandleTable{ owner_pid: table.owner_pid, count: table.count + 1, slots: slots }
    }
    if handle_slot_accepts_install(slots[1].state) {
        slots[1] = HandleSlot{ slot_id: slot_id, owner_pid: table.owner_pid, endpoint_id: endpoint_id, rights: rights, state: HandleState.Installed }
        return HandleTable{ owner_pid: table.owner_pid, count: table.count + 1, slots: slots }
    }
    if handle_slot_accepts_install(slots[2].state) {
        slots[2] = HandleSlot{ slot_id: slot_id, owner_pid: table.owner_pid, endpoint_id: endpoint_id, rights: rights, state: HandleState.Installed }
        return HandleTable{ owner_pid: table.owner_pid, count: table.count + 1, slots: slots }
    }
    if handle_slot_accepts_install(slots[3].state) {
        slots[3] = HandleSlot{ slot_id: slot_id, owner_pid: table.owner_pid, endpoint_id: endpoint_id, rights: rights, state: HandleState.Installed }
        return HandleTable{ owner_pid: table.owner_pid, count: table.count + 1, slots: slots }
    }
    if handle_slot_accepts_install(slots[4].state) {
        slots[4] = HandleSlot{ slot_id: slot_id, owner_pid: table.owner_pid, endpoint_id: endpoint_id, rights: rights, state: HandleState.Installed }
        return HandleTable{ owner_pid: table.owner_pid, count: table.count + 1, slots: slots }
    }
    return table
}

func remove_handle(table: HandleTable, slot_id: u32) HandleTable {
    index: usize = find_handle_index(table, slot_id)
    if index >= HANDLE_TABLE_CAPACITY || table.count == 0 {
        return table
    }
    slots: [5]HandleSlot = table.slots
    slots[index] = HandleSlot{ slot_id: slots[index].slot_id, owner_pid: slots[index].owner_pid, endpoint_id: slots[index].endpoint_id, rights: slots[index].rights, state: HandleState.Invalidated }
    return HandleTable{ owner_pid: table.owner_pid, count: table.count - 1, slots: slots }
}

func install_wait_handle(table: WaitTable, slot_id: u32, child_pid: u32) WaitTable {
    if table.count >= WAIT_TABLE_CAPACITY {
        return table
    }
    if wait_handle_installed(table, slot_id) {
        return table
    }
    slots: [2]WaitHandle = table.slots
    if wait_handle_state_score(slots[0].state) == 1 {
        slots[0] = WaitHandle{ slot_id: slot_id, owner_pid: table.owner_pid, child_pid: child_pid, exit_code: 0, signaled: 0, state: WaitHandleState.Installed }
        return WaitTable{ owner_pid: table.owner_pid, count: table.count + 1, slots: slots }
    }
    if wait_handle_state_score(slots[1].state) == 1 {
        slots[1] = WaitHandle{ slot_id: slot_id, owner_pid: table.owner_pid, child_pid: child_pid, exit_code: 0, signaled: 0, state: WaitHandleState.Installed }
        return WaitTable{ owner_pid: table.owner_pid, count: table.count + 1, slots: slots }
    }
    return table
}

func mark_wait_handle_exited(table: WaitTable, child_pid: u32, exit_code: i32) WaitTable {
    slots: [2]WaitHandle = table.slots
    if wait_handle_state_score(slots[0].state) == 2 && slots[0].child_pid == child_pid {
        slots[0] = WaitHandle{ slot_id: slots[0].slot_id, owner_pid: slots[0].owner_pid, child_pid: child_pid, exit_code: exit_code, signaled: 1, state: slots[0].state }
        return WaitTable{ owner_pid: table.owner_pid, count: table.count, slots: slots }
    }
    if wait_handle_state_score(slots[1].state) == 2 && slots[1].child_pid == child_pid {
        slots[1] = WaitHandle{ slot_id: slots[1].slot_id, owner_pid: slots[1].owner_pid, child_pid: child_pid, exit_code: exit_code, signaled: 1, state: slots[1].state }
        return WaitTable{ owner_pid: table.owner_pid, count: table.count, slots: slots }
    }
    return table
}

func find_child_for_wait_handle(table: WaitTable, slot_id: u32) u32 {
    index: usize = find_wait_index(table, slot_id)
    if index >= 2 {
        return 0
    }
    return table.slots[index].child_pid
}

func wait_handle_signaled(table: WaitTable, slot_id: u32) u32 {
    index: usize = find_wait_index(table, slot_id)
    if index >= 2 {
        return 0
    }
    return table.slots[index].signaled
}

func find_exit_code_for_wait_handle(table: WaitTable, slot_id: u32) i32 {
    index: usize = find_wait_index(table, slot_id)
    if index >= 2 {
        return 0
    }
    return table.slots[index].exit_code
}

func consume_wait_handle(table: WaitTable, slot_id: u32) WaitTable {
    index: usize = find_wait_index(table, slot_id)
    if index >= WAIT_TABLE_CAPACITY || table.count == 0 {
        return table
    }
    slots: [2]WaitHandle = table.slots
    slots[index] = empty_wait_handle()
    return WaitTable{ owner_pid: table.owner_pid, count: table.count - 1, slots: slots }
}

func find_endpoint_for_handle(table: HandleTable, slot_id: u32) u32 {
    index: usize = find_handle_index(table, slot_id)
    if index >= HANDLE_TABLE_CAPACITY {
        return 0
    }
    return table.slots[index].endpoint_id
}

func find_rights_for_handle(table: HandleTable, slot_id: u32) u32 {
    index: usize = find_handle_index(table, slot_id)
    if index >= HANDLE_TABLE_CAPACITY {
        return 0
    }
    return table.slots[index].rights
}

func resolve_endpoint_handle(table: HandleTable, slot_id: u32) EndpointHandleResolution {
    return resolve_endpoint_handle_for_rights(table, slot_id, 0)
}

func resolve_send_endpoint_handle(table: HandleTable, slot_id: u32) EndpointHandleResolution {
    return resolve_endpoint_handle_for_rights(table, slot_id, RIGHTS_ENDPOINT_SEND)
}

func resolve_receive_endpoint_handle(table: HandleTable, slot_id: u32) EndpointHandleResolution {
    return resolve_endpoint_handle_for_rights(table, slot_id, RIGHTS_ENDPOINT_RECEIVE)
}

func resolve_endpoint_handle_for_rights(table: HandleTable, slot_id: u32, required_rights: u32) EndpointHandleResolution {
    endpoint_id: u32 = find_endpoint_for_handle(table, slot_id)
    rights: u32 = find_rights_for_handle(table, slot_id)
    if endpoint_id == 0 {
        return EndpointHandleResolution{ endpoint_id: 0, valid: 0 }
    }
    if !endpoint_rights_include(rights, required_rights) {
        return EndpointHandleResolution{ endpoint_id: 0, valid: 0 }
    }
    return EndpointHandleResolution{ endpoint_id: endpoint_id, valid: 1 }
}

func handle_carries_endpoint_authority(table: HandleTable, slot_id: u32, endpoint_id: u32, expected_rights: u32) bool {
    resolved: EndpointHandleResolution = resolve_endpoint_handle_for_rights(table, slot_id, expected_rights)
    if resolved.valid == 0 || resolved.endpoint_id != endpoint_id {
        return false
    }
    return find_rights_for_handle(table, slot_id) == expected_rights
}

func resolve_attached_transfer_handle(table: HandleTable, attached_handle_slot: u32, attached_handle_count: usize) AttachedTransferResolution {
    if attached_handle_count == 0 {
        return AttachedTransferResolution{ attached_endpoint_id: 0, attached_rights: 0, attached_source_handle_slot: 0, attached_count: 0, valid: 1 }
    }
    if attached_handle_count != 1 {
        return AttachedTransferResolution{ attached_endpoint_id: 0, attached_rights: 0, attached_source_handle_slot: 0, attached_count: attached_handle_count, valid: 0 }
    }
    resolved: EndpointHandleResolution = resolve_endpoint_handle_for_rights(table, attached_handle_slot, RIGHTS_ENDPOINT_TRANSFER)
    attached_rights: u32 = find_transfer_rights_for_handle(table, attached_handle_slot)
    if resolved.valid == 0 || attached_rights == 0 {
        return AttachedTransferResolution{ attached_endpoint_id: 0, attached_rights: 0, attached_source_handle_slot: attached_handle_slot, attached_count: attached_handle_count, valid: 0 }
    }
    return AttachedTransferResolution{ attached_endpoint_id: resolved.endpoint_id, attached_rights: attached_rights, attached_source_handle_slot: attached_handle_slot, attached_count: attached_handle_count, valid: 1 }
}

func install_received_endpoint_handle(table: HandleTable, receive_handle_slot: u32, attached_count: usize, attached_endpoint_id: u32, attached_rights: u32) ReceivedHandleInstall {
    if attached_count == 0 {
        return ReceivedHandleInstall{ handle_table: table, received_handle_slot: 0, received_handle_count: 0, valid: 1 }
    }
    if receive_handle_slot == 0 {
        return ReceivedHandleInstall{ handle_table: table, received_handle_slot: 0, received_handle_count: 0, valid: 0 }
    }
    updated_table: HandleTable = install_endpoint_handle(table, receive_handle_slot, attached_endpoint_id, attached_rights)
    if !handle_install_succeeded(table, updated_table, receive_handle_slot) {
        return ReceivedHandleInstall{ handle_table: table, received_handle_slot: 0, received_handle_count: 0, valid: 0 }
    }
    return ReceivedHandleInstall{ handle_table: updated_table, received_handle_slot: receive_handle_slot, received_handle_count: attached_count, valid: 1 }
}

func admit_spawn_wait_handle(table: WaitTable, wait_handle_slot: u32, child_pid: u32) SpawnWaitHandleAdmission {
    if wait_handle_slot == 0 {
        return SpawnWaitHandleAdmission{ wait_table: table, valid: 0, invalid_handle: 1, exhausted: 0 }
    }
    updated_wait_table: WaitTable = install_wait_handle(table, wait_handle_slot, child_pid)
    if !wait_install_succeeded(table, updated_wait_table, wait_handle_slot) {
        return SpawnWaitHandleAdmission{ wait_table: table, valid: 0, invalid_handle: 0, exhausted: 1 }
    }
    return SpawnWaitHandleAdmission{ wait_table: updated_wait_table, valid: 1, invalid_handle: 0, exhausted: 0 }
}