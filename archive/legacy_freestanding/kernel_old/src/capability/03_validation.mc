func validate_syscall_capability_boundary() bool {
    table: HandleTable = handle_table_for_owner(7)
    table = install_endpoint_handle(table, 1, 11, RIGHTS_ENDPOINT_ALL)
    resolved: EndpointHandleResolution = resolve_endpoint_handle_for_rights(table, 1, RIGHTS_ENDPOINT_SEND)
    if resolved.valid == 0 {
        return false
    }
    if resolved.endpoint_id != 11 {
        return false
    }

    attached: AttachedTransferResolution = resolve_attached_transfer_handle(table, 1, 1)
    if attached.valid == 0 {
        return false
    }
    if attached.attached_endpoint_id != 11 {
        return false
    }
    if attached.attached_rights != RIGHTS_ENDPOINT_ALL {
        return false
    }

    invalidated: HandleTable = remove_handle(table, 1)
    if !handle_slot_invalidated(invalidated, 1) {
        return false
    }
    invalidated_lookup: EndpointHandleResolution = resolve_endpoint_handle_for_rights(invalidated, 1, RIGHTS_ENDPOINT_SEND)
    if invalidated_lookup.valid != 0 {
        return false
    }

    send_only: HandleTable = handle_table_for_owner(9)
    send_only = install_endpoint_handle(send_only, 2, 17, RIGHTS_ENDPOINT_SEND)
    if resolve_endpoint_handle_for_rights(send_only, 2, RIGHTS_ENDPOINT_SEND).valid == 0 {
        return false
    }
    if resolve_endpoint_handle_for_rights(send_only, 2, RIGHTS_ENDPOINT_RECEIVE).valid != 0 {
        return false
    }

    receiver: HandleTable = handle_table_for_owner(8)
    installed: ReceivedHandleInstall = install_received_endpoint_handle(receiver, 3, 1, attached.attached_endpoint_id, attached.attached_rights)
    if installed.valid == 0 {
        return false
    }
    if installed.received_handle_slot != 3 {
        return false
    }
    if find_endpoint_for_handle(installed.handle_table, 3) != 11 {
        return false
    }

    waits: WaitTable = wait_table_for_owner(7)
    admitted: SpawnWaitHandleAdmission = admit_spawn_wait_handle(waits, 1, 22)
    if admitted.valid == 0 {
        return false
    }
    if find_child_for_wait_handle(admitted.wait_table, 1) != 22 {
        return false
    }
    return true
}

func handle_state_score(state: HandleState) i32 {
    switch state {
    case HandleState.Empty:
        return 1
    case HandleState.Installed:
        return 2
    case HandleState.Invalidated:
        return 4
    default:
        return 0
    }
    return 0
}

func wait_handle_state_score(state: WaitHandleState) i32 {
    switch state {
    case WaitHandleState.Empty:
        return 1
    case WaitHandleState.Installed:
        return 2
    default:
        return 0
    }
    return 0
}

func kind_score(kind: CapabilityKind) i32 {
    switch kind {
    case CapabilityKind.Empty:
        return 1
    case CapabilityKind.Root:
        return 2
    case CapabilityKind.InitProgram:
        return 4
    case CapabilityKind.Endpoint:
        return 8
    default:
        return 0
    }
    return 0
}