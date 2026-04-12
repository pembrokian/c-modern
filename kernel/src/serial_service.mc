import syscall

enum SerialMessageTag {
    None,
    Ingress,
}

struct SerialServiceState {
    owner_pid: u32
    endpoint_handle_slot: u32
    last_tag: SerialMessageTag
    handled_ingress_count: usize
    last_source_pid: u32
    last_endpoint_id: u32
    last_payload_len: usize
    last_received_byte: u8
}

struct SerialIngressObservation {
    service_pid: u32
    source_pid: u32
    endpoint_id: u32
    payload_len: usize
    received_byte: u8
    ingress_count: usize
}

func service_state(owner_pid: u32, endpoint_handle_slot: u32) SerialServiceState {
    return SerialServiceState{ owner_pid: owner_pid, endpoint_handle_slot: endpoint_handle_slot, last_tag: SerialMessageTag.None, handled_ingress_count: 0, last_source_pid: 0, last_endpoint_id: 0, last_payload_len: 0, last_received_byte: 0 }
}

func record_ingress(state: SerialServiceState, observation: syscall.ReceiveObservation) SerialServiceState {
    received_byte: u8 = 0
    if observation.payload_len != 0 {
        received_byte = observation.payload[0]
    }
    return SerialServiceState{ owner_pid: state.owner_pid, endpoint_handle_slot: state.endpoint_handle_slot, last_tag: SerialMessageTag.Ingress, handled_ingress_count: state.handled_ingress_count + 1, last_source_pid: observation.source_pid, last_endpoint_id: observation.endpoint_id, last_payload_len: observation.payload_len, last_received_byte: received_byte }
}

func empty_ingress_observation() SerialIngressObservation {
    return SerialIngressObservation{ service_pid: 0, source_pid: 0, endpoint_id: 0, payload_len: 0, received_byte: 0, ingress_count: 0 }
}

func observe_ingress(state: SerialServiceState) SerialIngressObservation {
    return SerialIngressObservation{ service_pid: state.owner_pid, source_pid: state.last_source_pid, endpoint_id: state.last_endpoint_id, payload_len: state.last_payload_len, received_byte: state.last_received_byte, ingress_count: state.handled_ingress_count }
}

func tag_score(tag: SerialMessageTag) i32 {
    switch tag {
    case SerialMessageTag.None:
        return 1
    case SerialMessageTag.Ingress:
        return 2
    default:
        return 0
    }
    return 0
}