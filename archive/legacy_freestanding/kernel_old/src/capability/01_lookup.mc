func find_handle_index(table: HandleTable, slot_id: u32) usize {
    if handle_state_score(table.slots[0].state) == 2 && table.slots[0].slot_id == slot_id {
        return 0
    }
    if handle_state_score(table.slots[1].state) == 2 && table.slots[1].slot_id == slot_id {
        return 1
    }
    if handle_state_score(table.slots[2].state) == 2 && table.slots[2].slot_id == slot_id {
        return 2
    }
    if handle_state_score(table.slots[3].state) == 2 && table.slots[3].slot_id == slot_id {
        return 3
    }
    if handle_state_score(table.slots[4].state) == 2 && table.slots[4].slot_id == slot_id {
        return 4
    }
    return HANDLE_NOT_FOUND
}

func find_wait_index(table: WaitTable, slot_id: u32) usize {
    if wait_handle_state_score(table.slots[0].state) == 2 && table.slots[0].slot_id == slot_id {
        return 0
    }
    if wait_handle_state_score(table.slots[1].state) == 2 && table.slots[1].slot_id == slot_id {
        return 1
    }
    return WAIT_NOT_FOUND
}

func handle_slot_installed(table: HandleTable, slot_id: u32) bool {
    return find_handle_index(table, slot_id) < HANDLE_TABLE_CAPACITY
}

func wait_handle_installed(table: WaitTable, slot_id: u32) bool {
    return find_wait_index(table, slot_id) < WAIT_TABLE_CAPACITY
}

func handle_install_succeeded(before: HandleTable, after: HandleTable, slot_id: u32) bool {
    if after.count != before.count + 1 {
        return false
    }
    return handle_slot_installed(after, slot_id)
}

func handle_slot_invalidated(table: HandleTable, slot_id: u32) bool {
    if table.slots[0].slot_id == slot_id && handle_state_score(table.slots[0].state) == 4 {
        return true
    }
    if table.slots[1].slot_id == slot_id && handle_state_score(table.slots[1].state) == 4 {
        return true
    }
    if table.slots[2].slot_id == slot_id && handle_state_score(table.slots[2].state) == 4 {
        return true
    }
    if table.slots[3].slot_id == slot_id && handle_state_score(table.slots[3].state) == 4 {
        return true
    }
    if table.slots[4].slot_id == slot_id && handle_state_score(table.slots[4].state) == 4 {
        return true
    }
    return false
}

func wait_install_succeeded(before: WaitTable, after: WaitTable, slot_id: u32) bool {
    if after.count != before.count + 1 {
        return false
    }
    return wait_handle_installed(after, slot_id)
}

func handle_remove_succeeded(before: HandleTable, after: HandleTable, slot_id: u32) bool {
    if before.count == 0 {
        return false
    }
    if after.count + 1 != before.count {
        return false
    }
    return !handle_slot_installed(after, slot_id)
}