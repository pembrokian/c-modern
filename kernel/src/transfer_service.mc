import endpoint
import syscall

enum CapabilityTransferTag {
    None,
    Grant,
    Emit,
}

struct TransferServiceState {
    owner_pid: u32
    control_handle_slot: u32
    transferred_handle_slot: u32
    last_tag: CapabilityTransferTag
    grant_count: usize
    emit_count: usize
    last_client_pid: u32
    last_control_endpoint_id: u32
    last_transferred_endpoint_id: u32
    last_transferred_rights: u32
    last_grant_len: usize
    last_grant_byte0: u8
    last_grant_byte1: u8
    last_grant_byte2: u8
    last_grant_byte3: u8
    last_emit_len: usize
    last_emit_byte0: u8
    last_emit_byte1: u8
    last_emit_byte2: u8
    last_emit_byte3: u8
}

struct TransferObservation {
    service_pid: u32
    client_pid: u32
    control_endpoint_id: u32
    transferred_endpoint_id: u32
    transferred_rights: u32
    tag: CapabilityTransferTag
    grant_len: usize
    grant_byte0: u8
    grant_byte1: u8
    grant_byte2: u8
    grant_byte3: u8
    emit_len: usize
    emit_byte0: u8
    emit_byte1: u8
    emit_byte2: u8
    emit_byte3: u8
    grant_count: usize
    emit_count: usize
}

func service_state(owner_pid: u32, control_handle_slot: u32, transferred_handle_slot: u32) TransferServiceState {
    return TransferServiceState{ owner_pid: owner_pid, control_handle_slot: control_handle_slot, transferred_handle_slot: transferred_handle_slot, last_tag: CapabilityTransferTag.None, grant_count: 0, emit_count: 0, last_client_pid: 0, last_control_endpoint_id: 0, last_transferred_endpoint_id: 0, last_transferred_rights: 0, last_grant_len: 0, last_grant_byte0: 0, last_grant_byte1: 0, last_grant_byte2: 0, last_grant_byte3: 0, last_emit_len: 0, last_emit_byte0: 0, last_emit_byte1: 0, last_emit_byte2: 0, last_emit_byte3: 0 }
}

func record_grant(state: TransferServiceState, observation: syscall.ReceiveObservation, transferred_endpoint_id: u32, transferred_rights: u32) TransferServiceState {
    grant_byte0: u8 = 0
    grant_byte1: u8 = 0
    grant_byte2: u8 = 0
    grant_byte3: u8 = 0
    if observation.payload_len != 0 {
        grant_byte0 = observation.payload[0]
    }
    if observation.payload_len > 1 {
        grant_byte1 = observation.payload[1]
    }
    if observation.payload_len > 2 {
        grant_byte2 = observation.payload[2]
    }
    if observation.payload_len > 3 {
        grant_byte3 = observation.payload[3]
    }
    return TransferServiceState{ owner_pid: state.owner_pid, control_handle_slot: state.control_handle_slot, transferred_handle_slot: state.transferred_handle_slot, last_tag: CapabilityTransferTag.Grant, grant_count: state.grant_count + 1, emit_count: state.emit_count, last_client_pid: observation.source_pid, last_control_endpoint_id: observation.endpoint_id, last_transferred_endpoint_id: transferred_endpoint_id, last_transferred_rights: transferred_rights, last_grant_len: observation.payload_len, last_grant_byte0: grant_byte0, last_grant_byte1: grant_byte1, last_grant_byte2: grant_byte2, last_grant_byte3: grant_byte3, last_emit_len: state.last_emit_len, last_emit_byte0: state.last_emit_byte0, last_emit_byte1: state.last_emit_byte1, last_emit_byte2: state.last_emit_byte2, last_emit_byte3: state.last_emit_byte3 }
}

func emit_payload(state: TransferServiceState) [4]u8 {
    payload: [4]u8 = endpoint.zero_payload()
    payload[0] = state.last_grant_byte0
    payload[1] = state.last_grant_byte1
    payload[2] = state.last_grant_byte2
    payload[3] = state.last_grant_byte3
    return payload
}

func record_emit(state: TransferServiceState, observation: syscall.ReceiveObservation) TransferServiceState {
    emit_byte0: u8 = 0
    emit_byte1: u8 = 0
    emit_byte2: u8 = 0
    emit_byte3: u8 = 0
    if observation.payload_len != 0 {
        emit_byte0 = observation.payload[0]
    }
    if observation.payload_len > 1 {
        emit_byte1 = observation.payload[1]
    }
    if observation.payload_len > 2 {
        emit_byte2 = observation.payload[2]
    }
    if observation.payload_len > 3 {
        emit_byte3 = observation.payload[3]
    }
    return TransferServiceState{ owner_pid: state.owner_pid, control_handle_slot: state.control_handle_slot, transferred_handle_slot: state.transferred_handle_slot, last_tag: CapabilityTransferTag.Emit, grant_count: state.grant_count, emit_count: state.emit_count + 1, last_client_pid: state.last_client_pid, last_control_endpoint_id: state.last_control_endpoint_id, last_transferred_endpoint_id: state.last_transferred_endpoint_id, last_transferred_rights: state.last_transferred_rights, last_grant_len: state.last_grant_len, last_grant_byte0: state.last_grant_byte0, last_grant_byte1: state.last_grant_byte1, last_grant_byte2: state.last_grant_byte2, last_grant_byte3: state.last_grant_byte3, last_emit_len: observation.payload_len, last_emit_byte0: emit_byte0, last_emit_byte1: emit_byte1, last_emit_byte2: emit_byte2, last_emit_byte3: emit_byte3 }
}

func observe_transfer(state: TransferServiceState) TransferObservation {
    return TransferObservation{ service_pid: state.owner_pid, client_pid: state.last_client_pid, control_endpoint_id: state.last_control_endpoint_id, transferred_endpoint_id: state.last_transferred_endpoint_id, transferred_rights: state.last_transferred_rights, tag: state.last_tag, grant_len: state.last_grant_len, grant_byte0: state.last_grant_byte0, grant_byte1: state.last_grant_byte1, grant_byte2: state.last_grant_byte2, grant_byte3: state.last_grant_byte3, emit_len: state.last_emit_len, emit_byte0: state.last_emit_byte0, emit_byte1: state.last_emit_byte1, emit_byte2: state.last_emit_byte2, emit_byte3: state.last_emit_byte3, grant_count: state.grant_count, emit_count: state.emit_count }
}

func tag_score(tag: CapabilityTransferTag) i32 {
    switch tag {
    case CapabilityTransferTag.None:
        return 1
    case CapabilityTransferTag.Grant:
        return 2
    case CapabilityTransferTag.Emit:
        return 4
    default:
        return 0
    }
    return 0
}